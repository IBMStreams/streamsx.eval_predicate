/*
==============================================
# Licensed Materials - Property of IBM
# Copyright IBM Corp. 2021
==============================================
*/
/*
============================================================
First created on: Mar/05/2021
Last modified on: Sep/03/2021

This toolkit's public GitHub URL:
https://github.com/IBMStreams/streamsx.eval_predicate

This file contains C++ native function(s) provided in the
eval_predicate toolkit. It has very elaborate logic to
evaluate a user given expression a.k.a user defined rule.
It differs from the IBM Streams built-in function
evalPredicate in the following ways.

1) This new eval_predicate function allows the user
given expression (rule) to access map elements.

2) This new eval_predicate function allows the user
given expression (rule) to access nested tuple attributes.

3) This new eval_predicate function allows the user
given expression (rule) to have operational verbs such as
contains, startsWith, endsWith, notContains, notStartsWith,
notEndsWith, in. For case insensitive (CI) string operations, these
operational verbs can be used: containsCI, startsWithCI,
endsWithCI, inCI, equalsCI, notContainsCI, notStartsWithCI,
notEndsWithCI, notEqualsCI.
For checking the size of the set, list and map, these
operational verbs can be used: sizeEQ, sizeNE, sizeLT,
sizeLE, sizeGT, sizeGE

4) This new eval_predicate function supports the following operations.
--> It supports these relational operations: ==, !=, <, <=, >, >=
--> It supports these logical operations: ||, &&
--> It supports these arithmetic operations: +, -, *, /, %
--> It supports these special operations for rstring, set, list and map:
    contains, startsWith, endsWith, notContains, notStartsWith,
    notEndsWith, in, containsCI, startsWithCI, endsWithCI,
    inCI, equalsCI, notContainsCI, notStartsWithCI,
    notEndsWithCI, notEqualsCI,
    sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT, sizeGE
--> No bitwise operations are supported at this time.

5) Following are the data types currently allowed in an expression (rule).
If you need more data types to be allowed, please create an
issue in the IBMStreams GitHub or contact the author of this toolkit.

boolean, int32, uint32, int64, uint64, float32, float64, rstring,
set<int32>, set<int64>, set<float32>, set<float64>, set<rstring>,
list<int32>, list<int64>, list<float32>, list<float64>, list<rstring>, list<TUPLE>,
map<rstring,int32>, map<int32,rstring>, map<rstring,int64>, map<int64,rstring>,
map<rstring,float32>, map<float32,rstring>, map<rstring,float64>,
map<float64,rstring>, map<rstring,rstring>, map<int32,int32>, map<int32,int64>,
map<int64,int32>, map<int64,int64>, map<int32,float32>, map<int32,float64>,
map<int64,float32>, map<int64,float64>, map<float32,int32>, map<float32,int64>,
map<float64,int32>, map<float64,int64>, map<float32,float32>, map<float32,float64>,
map<float64,float32>, map<float64,float64> and nested tuple references
pointing to any of the attributes made using the types shown above.

Following are three tuple examples with varying degree of complexity.
1) tuple<rstring symbol,float32 price,uint32 quantity,boolean buyOrSell>
2) tuple<tuple<rstring name,rstring title,int32 id,rstring gender,set<rstring> skills> employee,tuple<rstring name,rstring id,rstring manager,int32 employeeCnt> department>
3) tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>

Following are the examples of expressions that can be sent for evaluation.
We support either zero or single level or multilevel (nested) parenthesis combination.
Within a given subexpression, you must use the same logical operators.

Zero parenthesis is used in this expression:
a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3

Single level parenthesis is used within each subexpression:
(a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
(a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)

Following expressions use nested parenthesis.
(a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)
(a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))
(a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
(a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))
((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)
(((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')

Note
----
The evalPredicate built-in function that is already available in
the IBM Streams product is implemented using C++ logic that is
tightly interwoven into the SPL programming model. It uses the
C++ boost spirit classic library for parsing the expressions and
then mapping them into the tuple attributes again via other modules
that are already part of the SPL runtime code. That one was designed
and built at least by 4 former colleagues over a period of 8 years.
To make changes in that existing implementation with thousands of
lines of comment free complex C++ logic proved to be a tall order for me.
So, I took a fresh approach to write this new eval_predicate function
with my own ideas along with sufficient commentary to explain the logic.
That obviously led to implementation and feature differences between
the built-in evalPredicate and my new eval_predicate in this file.
As this toolkit evolved, very good features got added to make it a
great rule processing engine within the IBM Streams product.
If positioned properly, this can be a difference maker for IBM Streams
in the competitive marketplace. It will also help customers and partners
in creating useful and powerful custom rule processors for many
use cases in different business domains.
============================================================
*/
#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_

// Include this SPL file so that we can use the SPL functions and types in this C++ code.
#include <SPL/Runtime/Function/SPLFunctions.h>
#include <SPL/Runtime/Type/SPLType.h>
#include <SPL/Runtime/Type/Tuple.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <stack>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <tr1/unordered_map>

// ====================================================================
// All the constants are defined here. It covers all the
// error codes returned by the eval_predicate function.
#define ALL_CLEAR 0
#define EMPTY_EXPRESSION 1
#define MISSING_OPEN_TUPLE_TAG 2
#define MISSING_CLOSE_ANGLE_BRACKET_FOR_NESTED_TUPLE1 3
#define MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_AFTER_TUPLE_ATTRIBUTE_NAME 4
#define MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_FOR_NESTED_TUPLE2 5
#define MISSING_SPACE_BEFORE_TUPLE_ATTRIBUTE_NAME 6
#define MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_AFTER_TUPLE_ATTRIBUTE_NAME2 7
#define INVALID_CHARACTER_FOUND_IN_EXPRESSION 8
#define UNMATCHED_CLOSE_PARENTHESIS_IN_EXPRESSION1 9
#define UNMATCHED_CLOSE_PARENTHESIS_IN_EXPRESSION2 10
#define UNMATCHED_CLOSE_BRACKET_IN_EXPRESSION1 11
#define UNMATCHED_CLOSE_BRACKET_IN_EXPRESSION2 12
#define UNMATCHED_OPEN_PARENTHESIS_OR_SQUARE_BRACKET_IN_EXPRESSION 13
#define PERIOD_CHARACTER_FOUND_OUTSIDE_OF_LHS_AND_RHS 14
#define EQUAL_CHARACTER_WITHOUT_AN_LHS 15
#define LHS_NOT_MATCHING_WITH_ANY_TUPLE_ATTRIBUTE 16
#define OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST 17
#define CLOSE_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST 18
#define ALL_NUMERALS_NOT_FOUND_AS_LIST_INDEX 19
#define OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_MAP 20
#define UNSUPPORTED_KEY_TYPE_FOUND_IN_MAP 21
#define SPACE_MIXED_WITH_NUMERALS_IN_LIST_INDEX 22
#define ALL_NUMERALS_NOT_FOUND_IN_INT_MAP_KEY 23
#define CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_INT_MAP_KEY 24
#define SPACE_MIXED_WITH_NUMERALS_IN_INT_MAP_KEY 25
#define MISSING_DECIMAL_POINT_IN_FLOAT_MAP_KEY 26
#define MORE_THAN_ONE_DECIMAL_POINT_IN_FLOAT_MAP_KEY 27
#define SPACE_MIXED_WITH_NUMERALS_IN_FLOAT_MAP_KEY 28
#define ALL_NUMERALS_NOT_FOUND_IN_FLOAT_MAP_KEY 29
#define CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_FLOAT_MAP_KEY 30
#define MISSING_OPEN_QUOTE_IN_STRING_MAP_KEY 31
#define MISSING_CLOSE_QUOTE_IN_STRING_MAP_KEY 32
#define INVALID_CHAR_FOUND_IN_STRING_MAP_KEY 33
#define CHAR_FOUND_AFTER_CLOSE_QUOTE_IN_STRING_MAP_KEY 34
#define CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_STRING_MAP_KEY 35
#define EMPTY_STRING_MAP_KEY_FOUND 36
#define INVALID_OPERATION_VERB_FOUND_IN_EXPRESSION 37
#define INCOMPATIBLE_DOUBLE_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE 38
#define INCOMPATIBLE_NOT_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE 39
#define INCOMPATIBLE_LESS_THAN_OPERATION_FOR_LHS_ATTRIB_TYPE 40
#define INCOMPATIBLE_LESS_THAN_OR_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE 41
#define INCOMPATIBLE_GREATER_THAN_OPERATION_FOR_LHS_ATTRIB_TYPE 42
#define INCOMPATIBLE_GREATER_THAN_OR_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE 43
#define INCOMPATIBLE_ADD_OPERATION_FOR_LHS_ATTRIB_TYPE 44
#define INCOMPATIBLE_SUBTRACT_OPERATION_FOR_LHS_ATTRIB_TYPE 45
#define INCOMPATIBLE_MULTIPLY_OPERATION_FOR_LHS_ATTRIB_TYPE 46
#define INCOMPATIBLE_DIVIDE_OPERATION_FOR_LHS_ATTRIB_TYPE 47
#define INCOMPATIBLE_MOD_OPERATION_FOR_LHS_ATTRIB_TYPE 48
#define INCOMPATIBLE_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE 49
#define INCOMPATIBLE_STARTS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE 50
#define INCOMPATIBLE_ENDS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE 51
#define INCOMPATIBLE_NOT_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE 52
#define INCOMPATIBLE_NOT_STARTS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE 53
#define INCOMPATIBLE_NOT_ENDS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE 54
#define ALL_NUMERALS_NOT_FOUND_IN_ARITHMETIC_OPERAND 55
#define NO_DECIMAL_POINT_IN_FLOAT_ARITHMETIC_OPERAND 56
#define MORE_THAN_ONE_DECIMAL_POINT_IN_ARITHMETIC_OPERAND 57
#define NO_OPERATION_VERB_FOUND_AFTER_ARITHMETIC_OPERAND 58
#define INVALID_OPERATION_VERB_FOUND_AFTER_ARITHMETIC_OPERAND 59
#define DECIMAL_POINT_FOUND_IN_NON_FLOAT_ARITHMETIC_OPERAND 60
#define UNPROCESSED_PARENTHESIS_FOUND_IN_EXPRESSION 61
#define UNPROCESSED_LHS_FOUND_IN_EXPRESSION 62
#define UNPROCESSED_OPERATION_VERB_FOUND_IN_EXPRESSION 63
#define UNPROCESSED_RHS_FOUND_IN_EXPRESSION 64
#define CLOSE_PARENTHESIS_FOUND_WITH_ZERO_PENDING_OPEN_PARENTHESIS 65
#define RHS_VALUE_NO_MATCH_FOR_BOOLEAN_LHS_TYPE 66
#define RHS_VALUE_NO_MATCH_FOR_INTEGER_LHS_TYPE 67
#define NO_DECIMAL_POINT_IN_RHS_VALUE 68
#define MORE_THAN_ONE_DECIMAL_POINT_IN_RHS_VALUE 69
#define RHS_VALUE_NO_MATCH_FOR_FLOAT_LHS_TYPE 70
#define RHS_VALUE_WITH_MISSING_OPEN_QUOTE_NO_MATCH_FOR_STRING_LHS_TYPE 71
#define RHS_VALUE_WITH_MISSING_CLOSE_QUOTE_NO_MATCH_FOR_STRING_LHS_TYPE 72
#define NEGATIVE_SIGN_AT_WRONG_POSITION_OF_AN_RHS_INTEGER 73
#define MORE_THAN_ONE_NEGATIVE_SIGN_IN_AN_RHS_INTEGER 74
#define NEGATIVE_SIGN_AT_WRONG_POSITION_OF_AN_RHS_FLOAT 75
#define MORE_THAN_ONE_NEGATIVE_SIGN_IN_AN_RHS_FLOAT 76
#define NEGATIVE_SIGN_FOUND_IN_NON_INTEGER_NON_FLOAT_ARITHMETIC_OPERAND 77
#define NEGATIVE_SIGN_AT_WRONG_POSITION_IN_ARITHMETIC_OPERAND 78
#define NEGATIVE_SIGN_FOUND_IN_UNSIGNED_INTEGER_ARITHMETIC_OPERAND 79
#define EXPRESSION_WITH_NO_LHS_AND_OPERATION_VERB_AND_RHS 80
#define INCOMPLETE_EXPRESSION_ENDING_WITH_LOGICAL_OPERATOR 81
#define INVALID_LOGICAL_OPERATOR_FOUND_IN_EXPRESSION 82
#define OPEN_PARENTHESIS_FOUND_NOT_RIGHT_BEFORE_LHS 83
#define CLOSE_PARENTHESIS_FOUND_NOT_RIGHT_AFTER_RHS 84
#define NO_SPACE_OR_ANOTHER_OPEN_PARENTHESIS_BEFORE_NEW_OPEN_PARENTHESIS 85
#define NO_SPACE_OR_ANOTHER_CLOSE_PARENTHESIS_AFTER_NEW_CLOSE_PARENTHESIS 86
#define NO_SPACE_RIGHT_BEFORE_LOGICAL_OPERATOR 87
#define NO_SPACE_RIGHT_AFTER_LOGICAL_OPERATOR 88
#define NESTED_OPEN_PARENTHESIS_FOUND 89
#define NESTED_CLOSE_PARENTHESIS_FOUND 90
#define MIXED_LOGICAL_OPERATORS_FOUND_IN_SUBEXPRESSION 91
#define MIXED_LOGICAL_OPERATORS_FOUND_IN_INTER_SUBEXPRESSIONS 92
#define FIRST_OPEN_PARENTHESIS_OCCURS_AFTER_A_COMPLETED_SUBEXPRESSION 93
#define PARENTHESIS_NOT_USED_CONSISTENTLY_THROUGHOUT_THE_EXPRESSION 94
#define TUPLE_SCHEMA_MISMATCH_FOUND_IN_EXP_EVAL_PLAN_CACHE 95
#define TUPLE_LITERAL_SCHEMA_GENERATION_ERROR 96
#define EXP_EVAL_CACHE_OBJECT_CREATION_ERROR 97
#define EXP_EVAL_PLAN_OBJECT_CREATION_ERROR 98
#define ERROR_INSERTING_EVAL_PLAN_PTR_IN_CACHE 99
#define INVALID_RSTRING_OPERATION_VERB_FOUND_DURING_EXP_EVAL 100
#define INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE 101
#define INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE 102
#define THREE_TOKENS_NOT_FOUND_IN_ARITHMETIC_OPERATION_VERB 103
#define EMPTY_VALUE_FOUND_FOR_ARITHMETIC_OPERAND 104
#define EMPTY_VALUE_FOUND_FOR_POST_ARITHMETIC_OPERATION_VERB 105
#define INVALID_POST_ARITHMETIC_OPERATION_VERB_FOUND_DURING_EXP_EVAL 106
#define DIVIDE_BY_ZERO_ARITHMETIC_FOUND_DURING_EXP_EVAL 107
#define COLLECTION_ITEM_EXISTENCE_INVALID_OPERATION_VERB_FOUND_DURING_EXP_EVAL 108
#define RELATIONAL_OR_ARITHMETIC_INVALID_OPERATION_VERB_FOUND_DURING_EXP_EVAL 109
#define INCORRECT_NUMBER_OF_INTER_SUBEXPRESSION_LOGICAL_OPERATORS 110
#define ZERO_SUBEXPRESSIONS_MAP_KEYS_FOUND_DURING_EVAL 111
#define KEY_NOT_FOUND_IN_SUB_EXP_MAP_DURING_EVAL 112
#define EMPTY_SUB_EXP_LAYOUT_LIST_DURING_EVAL 113
#define LHS_ATTRIB_NAME_STOPS_ABRUPTLY_AT_THE_END_OF_THE_EXPRESSION 114
#define MIXED_LOGICAL_OPERATORS_FOUND_IN_NESTED_SUBEXPRESSIONS 115
#define MISSING_TWO_CLOSE_ANGLE_BRACKETS_AFTER_LIST_OF_TUPLE 116
#define OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST_OF_TUPLE 117
#define ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_VALIDATION 118
#define NO_PERIOD_FOUND_AFTER_LIST_OF_TUPLE 119
#define ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_EVALUATION 120
#define EXP_EVAL_PLAN_OBJECT_CREATION_ERROR_FOR_LIST_OF_TUPLE 121
#define SPACE_NOT_FOUND_AFTER_SPECIAL_OPERATION_VERB 122
#define INCOMPATIBLE_SIZE_EQ_OPERATION_FOR_LHS_ATTRIB_TYPE 123
#define INCOMPATIBLE_SIZE_NE_OPERATION_FOR_LHS_ATTRIB_TYPE 124
#define INCOMPATIBLE_SIZE_LT_OPERATION_FOR_LHS_ATTRIB_TYPE 125
#define INCOMPATIBLE_SIZE_LE_OPERATION_FOR_LHS_ATTRIB_TYPE 126
#define INCOMPATIBLE_SIZE_GT_OPERATION_FOR_LHS_ATTRIB_TYPE 127
#define INCOMPATIBLE_SIZE_GE_OPERATION_FOR_LHS_ATTRIB_TYPE 128
#define RHS_VALUE_NO_MATCH_FOR_SIZEXX_OPERATION_VERB 129
#define INVALID_COLLECTION_SIZE_CHECK_OPERATION_VERB_FOUND_DURING_EXP_EVAL 130
#define EMPTY_ATTRIBUTE_NAME_GIVEN_FOR_VALUE_FETCHING 131
#define NON_SPACE_CHARACTER_FOUND_AFTER_A_VALID_ATTRIBUTE_NAME 132
#define ATTRIBUTE_NAME_WITH_NO_VALID_CHARACTERS 133
#define ATTRIBUTE_NAME_NOT_GOOD_FOR_VALIDATION 134
#define EMPTY_ATTRIBUTE_NAME_LAYOUT_LIST_DURING_VALUE_FETCH 135
#define WRONG_TYPE_OF_ATTRIBUTE_PASSED_AS_FUNCTION_ARGUMENT_BY_CALLER 136
#define ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_VALUE_FETCH 137
#define UNSUPPORTED_EVAL_CONDITION_DETECTED 138
#define UNSUPPORTED_FETCH_ATTRIBUTE_VALUE_CONDITION_DETECTED 139
#define INCOMPATIBLE_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 140
#define INCOMPATIBLE_STARTS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 141
#define INCOMPATIBLE_ENDS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 142
#define INCOMPATIBLE_NOT_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 143
#define INCOMPATIBLE_NOT_STARTS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 144
#define INCOMPATIBLE_NOT_ENDS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 145
#define INCOMPATIBLE_IN_OPERATION_FOR_LHS_ATTRIB_TYPE 146
#define INCOMPATIBLE_IN_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 147
#define INCOMPATIBLE_EQUALS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 148
#define INCOMPATIBLE_NOT_EQUALS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE 149
#define UNABLE_TO_PARSE_RHS_VALUE 150
#define RHS_VALUE_WITH_MISSING_OPEN_BRACKET_NO_MATCH_FOR_IN_OR_IN_CI_OPVERB 151
#define RHS_VALUE_WITH_MISSING_CLOSE_BRACKET_NO_MATCH_FOR_IN_OR_IN_CI_OPVERB 152
#define INVALID_RHS_LIST_LITERAL_STRING_FOUND_FOR_IN_OR_IN_CI_OPVERB 153
// ====================================================================
// Define a C++ namespace that will contain our native function code.
namespace eval_predicate_functions {
	using namespace std;
	// By including this line, we will have access to the SPL namespace and anything defined within that.
	using namespace SPL;

	// ====================================================================
	// This is a crucial class definition that holds different
	// subexpressions found in the user given expression string.
	// It forms the basis for having a cache for the repeated
	// evaluation of a given expression. The main idea here is to
	// have a full evaluation plan made ready for use whenever
	// needed. That will allow us to evaluate a given expression string
	// using a readily available evaluation plan by executing its steps.
	// This class definition is akin to a blueprint that contains all the
	// details necessary to evaluate a given expression.
	//
	// Following is the class that represents the evaluation plan for a
	// given expression. In this class, we store the data structures
	// required to evaluate each subexpression present within a given
	// full expression string.
	//
	class ExpressionEvaluationPlan {
		public:
			// Destructor.
			~ExpressionEvaluationPlan() {
			}

			// Public getter methods of this class.
			rstring const & getExpression() {
				return(expression);
			}

			rstring const & getTupleSchema() {
				return(tupleSchema);
			}

			SPL::map<rstring, SPL::list<rstring> > const & getSubexpressionsMap() {
				return(subexpressionsMap);
			}

			SPL::list<rstring> const & getSubexpressionsMapKeys() {
				return(subexpressionsMapKeys);
			}

			SPL::map<rstring, rstring> const & getIntraNestedSubexpressionLogicalOperatorsMap() {
				return(intraNestedSubexpressionLogicalOperatorsMap);
			}

			SPL::list<rstring> const & getInterSubexpressionLogicalOperatorsList() {
				return(interSubexpressionLogicalOperatorsList);
			}

			// Public setter methods of this class.
			void setExpression(rstring const & expr) {
				expression = expr;
			}

			void setTupleSchema(rstring const & mySchema) {
				tupleSchema = mySchema;
			}

			void setSubexpressionsMap(SPL::map<rstring, SPL::list<rstring> > const & myMap) {
				subexpressionsMap = myMap;
			}

			void setSubexpressionsMapKeys(SPL::list<rstring> const & mapKeys) {
				subexpressionsMapKeys = mapKeys;
			}

			void setIntraNestedSubexpressionLogicalOperatorsMap(SPL::map<rstring, rstring> const & myMap) {
				intraNestedSubexpressionLogicalOperatorsMap = myMap;
			}

			void setInterSubexpressionLogicalOperatorsList(SPL::list<rstring> const & opsList) {
				interSubexpressionLogicalOperatorsList = opsList;
			}

		private:
			// Private member variables of this class.
			// The entire user given expression is stored in this variable.
			rstring expression;

			// The schema literal for the tuple associated with a fully
			// validated expression is stored in this variable.
			rstring tupleSchema;

			// This map contains the details about the different
			// subexpressions present in a fully validated expression.
			// It is important to understand the structure of this map which
			// is explained in great detail throughout this file.
			// One such explanation is available around line 643 of this file.
			SPL::map<rstring, SPL::list<rstring> > subexpressionsMap;

			// This list provides the subexpression map keys in sorted order.
			SPL::list<rstring> subexpressionsMapKeys;

			// This map contains the logical operators used within a subexpression.
			// Key for this map is the subexpression id and the value is the logical operator.
			SPL::map<rstring, rstring> intraNestedSubexpressionLogicalOperatorsMap;

			// This list contains the logical operators used in between
			// different subexpressions present in a user given expression string.
			SPL::list<rstring> interSubexpressionLogicalOperatorsList;
	};

	// This is the data type for the expression evaluation plan cache.
    // We assume that a common use of this function is to evaluate the
	// same expression on each tuple that comes to an operator.
	// We cache the results returned by the validateExpression function here,
	// because the difference in performance is close to 30x for
    // what we assume is a common use.
    typedef std::tr1::unordered_map<SPL::rstring, ExpressionEvaluationPlan*> ExpEvalCache;
    // This will give us a TLS (Thread Local Storage) for this pointer based
    // data structure to be available all the time within a PE's thread. A PE can
    // contain a single operator or multiple operators in case of operator fusion.
    // So, this static (global) variable is only applicable within a given thread that
    // is accessible either by one or more operators.
    static __thread ExpEvalCache* expEvalCache = NULL;

	// ====================================================================
	// Prototype for our native functions are declared here.
	//
    /// Evaluate a given SPL expression.
    /// @return the result of the evaluation
	template<class T1>
    boolean eval_predicate(rstring const & expr,
    	T1 const & myTuple, int32 & error, boolean trace);

	// ====================================================================
    // Prototype for other functions used only within this
    // C++ header file are declared here.
    //
    // Trace (display) the inside information for a given tuple.
    void traceTupleAtttributeNamesAndValues(Tuple const & myTuple,
    	SPL::map<rstring, rstring> & tupleAttributesMap, boolean trace);
    // Get the SPL literal string for a given tuple.
    // @return the SPL type name
    rstring getSPLTypeName(ConstValueHandle const & handle, boolean trace);
    // Get the parsed attribute names and their types for a given tuple literal string.
    boolean parseTupleAttributes(rstring const & myTupleSchema,
    	SPL::map<rstring, rstring> & tupleAttributesMap,
		int32 & error, boolean trace);
    // Validate the expression.
	// Note: The space below between > > is a must. Otherwise, compiler will give an error.
    boolean validateExpression(rstring const & expr,
    	SPL::map<rstring, rstring> const & tupleAttributesMap,
		SPL::map<rstring, SPL::list<rstring> > & subexpressionsMap,
		SPL::map<rstring, rstring> & intraNestedSubexpressionLogicalOperatorsMap,
		SPL::list<rstring> & interSubexpressionLogicalOperatorsList,
		int32 & error, int32 & validationStartIdx, boolean trace);
    // Evaluate the expression according to the predefined plan.
    boolean evaluateExpression(ExpressionEvaluationPlan *evalPlanPtr,
    	Tuple const & myTuple, int32 & error, boolean trace);
    // Check if a given quote character marks the end of a map key string.
    boolean isQuoteCharacterAtEndOfMapKeyString(blob const & myBlob, int32 const & idx);
    // Check if a given quote character marks the end of an RHS string.
    boolean isQuoteCharacterAtEndOfRhsString(blob const & myBlob, int32 const & idx);
    // Check if a given ] character marks the end of an RHS list string literal.
    boolean isCloseBracketAtEndOfRhsString(blob const & myBlob, int32 const & idx);
    // Get the constant value handle for a given attribute name in a given tuple.
    void getConstValueHandleForTupleAttribute(Tuple const & myTuple,
        rstring attributeName, ConstValueHandle & cvh);
    // Perform eval operations for an rstring based LHS attribute.
    void performRStringEvalOperations(rstring const & lhsValue,
    	rstring const & rhsValue, rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error);
    // Check if a given string represents a number (integer or float).
    boolean isNumber(rstring const & str);
    // Perform existence check eval operations for a collection based LHS attribute.
    void performCollectionItemExistenceEvalOperations(boolean const & itemExists,
    	rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error);
    // Perform size check eval operations for a collection based LHS attribute.
    void performCollectionSizeCheckEvalOperations(int32 const & lhsSize,
    	int32 const & rhsInt32, rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error);
    // Perform relational or arithmetic eval operations.
	template<class T1>
	void performRelationalOrArithmeticEvalOperations(T1 const & lhsValue,
	    T1 const & rhsValue, rstring const & operationVerb,
		T1 const & arithmeticOperandValue,
		rstring const & postArithmeticOperationVerb,
		boolean & subexpressionEvalResult, int32 & error);
	// Perform modulus arithmetic via overloaded functions.
	// Just because C++ doesn't support modulus for float values,
	// we have to take this approach of using overloaded functions.
	void calculateModulus(int32 const & lhsValue,
		int32 const & arithmeticOperandValue, int32 & result);
	void calculateModulus(uint32 const & lhsValue,
		uint32 const & arithmeticOperandValue, uint32 & result);
	void calculateModulus(int64 const & lhsValue,
		int64 const & arithmeticOperandValue, int64 & result);
	void calculateModulus(uint64 const & lhsValue,
		uint64 const & arithmeticOperandValue, uint64 & result);
	void calculateModulus(float32 const & lhsValue,
		float32 const & arithmeticOperandValue, float32 & result);
	void calculateModulus(float64 const & lhsValue,
		float64 const & arithmeticOperandValue, float64 & result);
	void calculateModulus(boolean const & lhsValue,
		boolean const & arithmeticOperandValue, boolean & result);
	// Perform post arithmetic eval operations.
	template<class T1>
	void performPostArithmeticEvalOperations(T1 const & arithmeticResult,
		T1 const & rhsValue, rstring const & postArithmeticOperationVerb,
		boolean & subexpressionEvalResult, int32 & error);
	// Create the next subexpression id.
	void getNextSubexpressionId(int32 const & currentNestedSubexpressionLevel,
		rstring & subexpressionId);
	// Check if the next non-space character is an open parenthesis.
	boolean isNextNonSpaceCharacterOpenParenthesis(blob const & myBlob,
		int32 const & idx, int32 const & stringLength);
	// Find if the current single subexpression is enclosed within a parenthesis.
	boolean isThisAnEnclosedSingleSubexpression(rstring const & expr,
		int32 const & idx);
	// Check if the next non-space character is a close parenthesis.
	boolean isNextNonSpaceCharacterCloseParenthesis(blob const & myBlob,
		int32 const & idx, int32 const & stringLength);
	// This method gets the relevant details about the
	// nested subexpression group.
	void getNestedSubexpressionGroupInfo(rstring const & subexpressionId,
		SPL::list<rstring> const & subexpressionIdsList,
		SPL::map<rstring, rstring> const & intraNestedSubexpressionLogicalOperatorsMap,
		int32 & subexpressionCntInCurrentNestedGroup,
		rstring & intraNestedSubexpressionLogicalOperator);
	// This method fetches the value of a user given
	// attribute present in a user given tuple.
	template<class T1, class T2>
	void get_tuple_attribute_value(rstring const & attributeName,
		T1 const & myTuple, T2 & value, int32 & error, boolean const & trace);
	// This method validates the user given attribute name for
	// its syntax correctness.
    boolean validateTupleAttributeName(rstring const & attributeName,
        SPL::map<rstring, rstring> const & tupleAttributesMap,
    	SPL::list<rstring> & attributeNameLayoutList,
    	int32 & error, int32 & validationStartIdx, boolean trace);
    // This method fetches the value of a given tuple attribute name.
    template<class T1, class T2>
    void fetchTupleAttributeValue(rstring const & attributeName,
    	SPL::map<rstring, rstring> const & tupleAttributesMap,
		SPL::list<rstring> const & attributeNameLayoutList,
		T1 const & myTuple, T2 & value, int32 & error, boolean trace);
    // ====================================================================

	// Evaluate a given expression.
    // Example expressions:
    // a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    // (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    // Allowed operators: logical, relational, arithmetic and special operators.
    //
	// Evaluate an expression.
	// Arg1: Expression
	// Arg2: Your tuple
	// Arg3: A mutable int32 variable to receive non-zero eval error code if any.
	// Arg4: A boolean value to enable debug tracing inside this function.
	// It returns true if the expression evaluation is successful.
    template<class T1>
    inline boolean eval_predicate(rstring const & expr,
    	T1 const & myTuple, int32 & error, boolean trace=false) {
	    boolean result = false;
    	error = ALL_CLEAR;

    	// Check if there is some content in the given expression.
    	if(Functions::String::length(expr) == 0) {
    		error = EMPTY_EXPRESSION;
    		return(false);
    	}

    	// Get the string literal of a give tuple.
		// Example of myTuple's schema:
		// myTuple=tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>
		//
    	SPLAPPTRC(L_TRACE, "Begin timing measurement 1", "TupleSchemaConstructor");
    	rstring myTupleSchema = getSPLTypeName(myTuple, trace);
    	SPLAPPTRC(L_TRACE, "End timing measurement 1", "TupleSchemaConstructor");

    	if(myTupleSchema == "") {
    		// This should never occur. If it happens in
    		// extremely rare cases, we have to investigate the
    		// tuple literal schema generation function.
    		error = TUPLE_LITERAL_SCHEMA_GENERATION_ERROR;
    		return(false);
    	}

	    if (expEvalCache == NULL) {
	    	// Create this only once per operator thread.
	    	expEvalCache = new ExpEvalCache;

	    	if(expEvalCache  == NULL) {
	    		// If we can't create the cache, then that is troublesome.
	    		error = EXP_EVAL_CACHE_OBJECT_CREATION_ERROR;
	    		return(false);
	    	}
	    }

	    // We can now check if the given expression is already in the eval plan cache.
	    ExpEvalCache::iterator it = expEvalCache->find(expr);

	    if (it != expEvalCache->end()) {
	    	// We found this expression in the cache.
	    	// Let us see if the tuple schema we originally stored in the
	    	// cache matches with the schema for the tuple that the
	    	// caller passed in this call to this native function.
	    	if(it->second->getTupleSchema() != myTupleSchema) {
	    		if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 2b ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Tuple schema mismatch found inside the expression evaluation plan cache." << endl;
					cout << "Tuple schema stored in the cache=" <<
						it->second->getTupleSchema() << endl;
					cout << "Schema for the tuple passed in this call=" << myTupleSchema << endl;
					cout << "Total number of expressions in the cache=" <<
						expEvalCache->size() << endl;
					cout << "==== END eval_predicate trace 2b ====" << endl;
	    		}

	    		error = TUPLE_SCHEMA_MISMATCH_FOUND_IN_EXP_EVAL_PLAN_CACHE;
	    		return(false);
	    	} else {
	    		if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 3b ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Matching tuple schema is found inside the expression evaluation plan cache." << endl;
					cout << "Total number of expressions in the cache=" <<
						expEvalCache->size() << endl;
					cout << "==== END eval_predicate trace 3b ====" << endl;
	    		}
	    	}

	    	// We will continue evaluating this expression outside of this if block.
	    } else {
    		if(trace == true) {
				cout << "==== BEGIN eval_predicate trace 2a ====" << endl;
				cout << "Full expression=" << expr << endl;
				cout << "Expression is not found inside the evaluation plan cache." << endl;
				cout << "Starting the preparation for adding it to the eval plan cache." << endl;
				cout << "Total number of expressions in the cache=" <<
					expEvalCache->size() << endl;
				cout << "==== END eval_predicate trace 2a ====" << endl;
    		}

	    	// This expression is not in the eval plan cache. So, we will do the
	    	// preparation necessary for adding it to the eval plan cache.
			// Let us parse the individual attributes of the given tuple and store them in a map.
			SPL::map<rstring, rstring> tupleAttributesMap;
			SPLAPPTRC(L_TRACE, "Begin timing measurement 2", "TupleAttributeParser");
			result = parseTupleAttributes(myTupleSchema,
				tupleAttributesMap, error, trace);
			SPLAPPTRC(L_TRACE, "End timing measurement 2", "TupleAttributeParser");

			if(result == false) {
				return(false);
			}

			// If trace is enabled let us do the introspection of the
			// user provided tuple and display its attribute names and values.
			traceTupleAtttributeNamesAndValues(myTuple, tupleAttributesMap, trace);

			// Note: The space below between > > is a must. Otherwise, compiler will give an error.
			// This map's key is a subexpression id.
			// Subexpression id will go something like this:
			// 1.1, 1.2, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.1, 4.2, 4.3, 5.1
			// Subexpression id is made of level 1 and level2.
			// We support either zero parenthesis or a single level or
			// multilevel (nested) parenthesis.
			// Logical operators used within a subexpression must be of the same kind.
			//
			// A few examples of zero, single and nested parenthesis.
			//
			// 1.1             2.1                 3.1           4.1
			// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
			//
			// 1.1                               2.1
			// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
			//
			// 1.1                2.1                   3.1             4.1
			// (a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)
			//
			// 1.1                          2.1                       2.2
			// (a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)
			//
			// 1.1                 2.1                       2.2
			// (a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))
			//
			// 1.1                                 2.1
			// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
			//
			// 1.1                     2.1                 2.2
			// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))
			//
			// 1.1                                        1.2                           2.1
			// ((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)
			//
			// 1.1                                                                   1.2                          1.3
			// (((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')
			//
			// This map's value is a list that describes the composition of a given subexpression.
			// Structure of such a list will go something like this:
			// This list will have a sequence of rstring items as shown below.
			// LHSAttribName
			// LHSAttribType
			// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
			// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
			// RHSValue
			// Intra subexpression logical operator - When N/A, it will have an empty string.
			// ...   - The sequence above repeats for this subexpression.
			//
			SPL::map<rstring, SPL::list<rstring> > subexpressionsMap;
			// Store the logical operators within the nested sub-expressions.
			// Key for this map will be id of the nested subexpression that
			// is preceding the logical operator. Value will be the logical
			// operator.
			SPL::map<rstring, rstring> intraNestedSubexpressionLogicalOperatorsMap;
			// Store the logical operators between different sub-expressions.
			// This list will have N-1 items where N is the total number of
			// subexpressions stored in the map above.
			SPL::list<rstring> interSubexpressionLogicalOperatorsList;

			// Let us validate the expression for correctness in its use of
			// the correct tuple attributes and correct operation verbs.
			// In addition to validating the expression, let us also
			// get back a reusable map structure of how the expression is made,
			// how it is tied to the tuple attributes and what operations
			// need to be performed later while evaluating the expression.
			SPLAPPTRC(L_TRACE, "Begin timing measurement 3", "ExpressionValidator");
			// Perform the validation from the beginning of
			// the expression starting at index 0.
			int32 validationStartIdx = 0;
			result = validateExpression(expr, tupleAttributesMap,
				subexpressionsMap,
				intraNestedSubexpressionLogicalOperatorsMap,
				interSubexpressionLogicalOperatorsList,
				error, validationStartIdx, trace);
			SPLAPPTRC(L_TRACE, "End timing measurement 3", "ExpressionValidator");

			if(result == false) {
				return(false);
			}

			// We have done a successful expression validation.
			// We can prepare to store the results from the
			// validation in a cache for reuse later if the
			// same expression is sent repeatedly for evaluation.
			//
			// Let us sort the subexpressions map keys so that
			// we can process them in the correct order.
			SPL::list<rstring> subexpressionsMapKeys =
				Functions::Collections::keys(subexpressionsMap);
			Functions::Collections::sortM(subexpressionsMapKeys);

			// We can now create a new eval plan cache entry for this expression.
			ExpressionEvaluationPlan *evalPlanPtr = NULL;
			evalPlanPtr = new ExpressionEvaluationPlan();

			if(evalPlanPtr == NULL) {
				error = EXP_EVAL_PLAN_OBJECT_CREATION_ERROR;
				return(false);
			}

			// Let us take a copy of various data structures related to this
			// fully validated expression in our eval plan cache for
			// prolonged use by calling the setter methods of the cache object.
			evalPlanPtr->setExpression(expr);
			evalPlanPtr->setTupleSchema(myTupleSchema);
			evalPlanPtr->setSubexpressionsMap(subexpressionsMap);
			evalPlanPtr->setSubexpressionsMapKeys(subexpressionsMapKeys);
			evalPlanPtr->setIntraNestedSubexpressionLogicalOperatorsMap(
				intraNestedSubexpressionLogicalOperatorsMap);
			evalPlanPtr->setInterSubexpressionLogicalOperatorsList(
				interSubexpressionLogicalOperatorsList);

			// Let us store it as a K/V pair in the map now.
	        std::pair<ExpEvalCache::iterator, bool> cacheInsertResult =
	        	expEvalCache->insert(std::make_pair(expr, evalPlanPtr));

	        if(cacheInsertResult.second == false) {
	        	error = ERROR_INSERTING_EVAL_PLAN_PTR_IN_CACHE;
	        	return(false);
	        }

    		if(trace == true) {
				cout << "==== BEGIN eval_predicate trace 11a ====" << endl;
				cout << "Full expression=" << expr << endl;
				cout << "Inserted the expression in the eval plan cache." << endl;
				cout << "Total number of expressions in the cache=" <<
					expEvalCache->size() << endl;
				cout << "==== END eval_predicate trace 11a ====" << endl;
    		}

	        it = cacheInsertResult.first;
	    } // End of the else block.

	    // We have a valid iterator from the eval plan cache for the given expression.
	    // We can go ahead and execute the evaluation plan now.
	    SPLAPPTRC(L_TRACE, "Begin timing measurement 4", "ExpressionEvaluation");
	    // We are making a non-recursive call.
	    result = evaluateExpression(it->second, myTuple, error, trace);
	    SPLAPPTRC(L_TRACE, "End timing measurement 4", "ExpressionEvaluation");

    	return(result);
    } // End of eval_predicate
    // ====================================================================

    // ====================================================================
    // This function receives a Tuple as input and returns a tuple literal string.
    // We will later parse the tuple literal string to create a map of all the
    // attributes in a user given tuple.
	// Example of myTuple's schema that will be returned by this function:
	// myTuple=tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>
	//
    inline rstring getSPLTypeName(ConstValueHandle const & handle, boolean trace=false) {
    	SPL::Meta::Type mtype = handle.getMetaType();

    	// Go through the meta type of every single attribute and
    	// form a tuple literal. This will have recursive calls into
    	// this same function when the user given tuple is nested.
    	switch(mtype) {
    		case Meta::Type::INVALID:
    			assert(!"cannot happen");
    			return "";
    		case Meta::Type::BOOLEAN:
    			return "boolean";
    		case Meta::Type::ENUM: {
    			Enum const & data = handle;
    			string res = "enum<";
    			vector<string> const & enums = data.getValidValues();
    			for (size_t i=0, iu=enums.size(); i<iu; ++i) {
    				if (i>0) {
    					res += ",";
    				}

    				res += enums[i];
    			}

    			res += ">";
    			return res;
    		}
    		case Meta::Type::INT8:
    			return "int8";
    		case Meta::Type::INT16:
    			return "int16";
    		case Meta::Type::INT32:
    			return "int32";
    		case Meta::Type::INT64:
    			return "int64";
    		case Meta::Type::UINT8:
    			return "uint8";
    		case Meta::Type::UINT16:
    			return "uint16";
    		case Meta::Type::UINT32:
    			return "uint32";
    		case Meta::Type::UINT64:
    			return "uint64";
    		case Meta::Type::FLOAT32:
    			return "float32";
    		case Meta::Type::FLOAT64:
    			return "float64";
    		case Meta::Type::DECIMAL32:
    			return "decimal32";
    		case Meta::Type::DECIMAL64:
    			return "decimal64";
    		case Meta::Type::DECIMAL128:
    			return "decimal128";
    		case Meta::Type::COMPLEX32:
    			return "complex32";
    		case Meta::Type::COMPLEX64:
    			return "complex64";
    		case Meta::Type::TIMESTAMP:
    			return "timestamp";
    		case Meta::Type::RSTRING:
    			return "rstring";
    		case Meta::Type::BSTRING: {
    			BString const & data = handle;
    			string res = "rstring[";
    			ostringstream ostr;
    			ostr << data.getBoundedSize();
    			res += ostr.str() + "]";
    			return res;
    		}
    		case Meta::Type::USTRING:
    			return "ustring";
    		case Meta::Type::BLOB:
    			return "blob";
    		case Meta::Type::LIST: {
    			List const & data = handle;
    			string res = "list<";
    			ValueHandle elem = data.createElement();
    			// Recursion
    			res += getSPLTypeName(elem);
    			elem.deleteValue();
    			res += ">";
    			return res;
    		}
    		case Meta::Type::BLIST: {
    			BList const & data = handle;
    			string res = "list<";
    			ValueHandle elem = data.createElement();
    			// Recursion
    			res += getSPLTypeName(elem);
    			elem.deleteValue();
    			res += ">[";
    			ostringstream ostr;
    			ostr << data.getBoundedSize();
    			res += ostr.str() + "]";
    			return res;
    		}
    		case Meta::Type::SET: {
    			Set const & data = handle;
    			string res = "set<";
    			ValueHandle elem = data.createElement();
    			// Recursion
    			res += getSPLTypeName(elem);
    			elem.deleteValue();
    			res += ">";
    			return res;
    		}
    		case Meta::Type::BSET: {
    			BSet const & data = handle;
    			string res = "set<";
    			ValueHandle elem = data.createElement();
    			// Recursion
    			res += getSPLTypeName(elem);
    			elem.deleteValue();
    			res += ">[";
    			ostringstream ostr;
    			ostr << data.getBoundedSize();
    			res += ostr.str() + "]";
    			return res;
    		}
    		case Meta::Type::MAP: {
    			Map const & data = handle;
    			string res = "map<";
    			ValueHandle key = data.createKey();
    			ValueHandle value = data.createValue();
    			res += getSPLTypeName(key);
    			res += ",";
    			// Recursion
    			res += getSPLTypeName(value);
    			key.deleteValue();
    			value.deleteValue();
    			res += ">";
    			return res;
    		}
    		case Meta::Type::BMAP: {
    			BMap const & data = handle;
    			string res = "map<";
    			ValueHandle key = data.createKey();
    			ValueHandle value = data.createValue();
    			// Recursion
    			res += getSPLTypeName(key);
    			res += ",";
    			// Recursion
    			res += getSPLTypeName(value);
    			key.deleteValue();
    			value.deleteValue();
    			res += ">[";
    			ostringstream ostr;
    			ostr << data.getBoundedSize();
    			res += ostr.str() + "]";
    			return res;
    		}
    		case Meta::Type::TUPLE: {
    			Tuple const & data = handle;
    			string res = "tuple<";

    			for (size_t i=0, iu=data.getNumberOfAttributes(); i<iu; ++i) {
    				if (i>0) {
    					res += ",";
    				}

    				ConstValueHandle attrb = data.getAttributeValue(i);
    				// Recursion.
    				res += getSPLTypeName(attrb);
    				res += " " + data.getAttributeName(i);

    				if(trace == true) {
						SPL::Meta::Type mtype2 = attrb.getMetaType();

						if(mtype2 == Meta::Type::TUPLE) {
							cout << "==== BEGIN eval_predicate trace 1 ====" << endl;
							cout << "i=" << i << ", iu=" << iu << ", mtype=" <<
								mtype2 << ", attr=" << data.getAttributeName(i) << endl;
							cout << "==== END eval_predicate trace 1 ====" << endl;
						}
    				}

    			}

    			res += ">";
    			return res;
    		}
    		case Meta::Type::XML:
    			return "xml";
    		default:
				assert(!"cannot happen");
				return "";
    	} // End of switch.
    } // End of getSPLTypeName
    // ====================================================================

    // ====================================================================
    // This function takes a tuple literal string as input and parses it
    // to fetch all its attribute names and their types as map elements.
    // It does it for flat as well as nested tuples. It takes the SPL::map
    // variable as an input via reference. It returns a boolean value to
    // indicate the result of the tuple attribute parsing.
    inline boolean parseTupleAttributes(rstring const & myTupleSchema,
    	SPL::map<rstring, rstring> & tupleAttributesMap,
		int32 & error, boolean trace=false) {
    	error = ALL_CLEAR;
		// Example of myTuple's schema:
		// myTuple=tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>
		//
    	if(trace == true) {
    		cout << "==== BEGIN eval_predicate trace 3a ====" << endl;
			cout << "myTupleSchema=" << myTupleSchema << endl;
			cout << "==== END eval_predicate trace 3a ====" << endl;
    	}

    	boolean initialTupleTokenFound = false;
    	int stringLength = Functions::String::length(myTupleSchema);
    	// Get a blob representation of it.
    	SPL::blob myBlob = Functions::String::convertToBlob(myTupleSchema);
    	SPL::list<rstring> nestedTupleNamesList;
    	int idx = 0;

    	while(idx < stringLength) {
    		if(initialTupleTokenFound == false) {
    			// We must find the first tuple< substring to proceed further.
    			if(Functions::String::findFirst(myTupleSchema, "tuple<", idx) != 0) {
    				// This is not a valid tuple sent by the caller.
    				error = MISSING_OPEN_TUPLE_TAG;
    				return(false);
    			}

    			initialTupleTokenFound = true;
    			// Move idx to just past the last character of the initial tuple< token.
    			idx += 6;
    			continue;
    		}

    		// Let us check if it is a nested tuple i.e. tuple<...>
    		if(Functions::String::findFirst(myTupleSchema, "tuple<", idx) == idx) {
    			// We encountered a nested tuple. Let us get the name of this tuple attribute.
    			// Move idx past the tuple< token i.e. position it after its open angle bracket.
    			idx += 6;
    			int idx2 = idx;
    			int32 angleBracketCnt = 1;

    			// Let us get the index for the end of this tuple i.e. its close angle bracket.
				for(int x=idx2; x<stringLength; x++) {
					if(myBlob[x] == '<') {
						// This is a < character.
						// This will be the case if the outer tuple has other
						// inner tuples or lists or sets or maps.
						angleBracketCnt++;
					} else if (myBlob[x] == '>') {
						// This is a > character.
						angleBracketCnt--;
					}

					if(angleBracketCnt == 0) {
						// We found the specific close angle bracket that we need.
						idx2 = x;
						break;
					}
				} // End of for loop.

				if(angleBracketCnt > 0) {
					error = MISSING_CLOSE_ANGLE_BRACKET_FOR_NESTED_TUPLE1;
					return(false);
				}

				// The attribute name of this nested tuple is just after the
				// close angle bracket where we are at now. Let us parse that tuple name.
				// Let us locate either the comma or an angle bracket after the tuple name.
				int32 idx3 = -1;

				for(int x=idx2+1; x<stringLength; x++) {
					if(myBlob[x] == ',' || myBlob[x] == '>') {
						// 44 is a comma character. 62 is a close angle bracket character.
						idx3 = x;
						break;
					}
				}

				if(idx3 == -1) {
					error = MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_AFTER_TUPLE_ATTRIBUTE_NAME;
					return(false);
				}

				// Let us get the tuple attribute name now.
				// idx2 is pointing at the close angle bracket. Add to skip the following
				// space character and point at the first character of the tuple name.
				rstring taName = Functions::String::substring(myTupleSchema, idx2+2, idx3-idx2-2);

				// Add the nested tuple name to the list for later use.
				Functions::Collections::appendM(nestedTupleNamesList, taName);
				continue;
    		} // End of parsing a nested tuple< token.

    		// Check if we are at a comma or at a close angle bracket.
    		if(myBlob[idx] == ',') {
    			// It is a comma character.
    			idx++;
    			continue;
    		}

    		if(myBlob[idx] == '>') {
    			// This is a > character.
    			// This > could be the very last character of the tuple schema or
    			// it could be followed by a space.
    			// Please look at the example schema given above.
    			// Depending on one of those conditions, we will do specific logic here.
    			idx++;
    			if(idx < stringLength && myBlob[idx] == ' ') {
    				// The character following the > is a space character.
    				// e-g: longitude> geo,
    				// e-g: humidity> weather> details,
    				// In this case, we will position it to the next comma or the
    				// close angle bracket character. We will take whichever character
    				// appears first.
    				int idx2 = Functions::String::findFirst(myTupleSchema, ",", idx);
    				int idx3 = Functions::String::findFirst(myTupleSchema, ">", idx);

    				if(idx2 == -1 && idx3 == -1) {
						error = MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_FOR_NESTED_TUPLE2;
						return(false);
    				}

    	    		if(idx2 == -1) {
    	    			idx2 = idx3;
    	    		}

    	    		// Consider whichever comes first.
    	    		if((idx3 != -1) && (idx3 < idx2)) {
    	    			idx2 = idx3;
    	    		}

    				idx = idx2;
    				// Since we completed the parsing of a nested tuple,
    				// we can remove it from the list.
    				// This should be the last item in the list.
    				Functions::Collections::removeM(nestedTupleNamesList,
    					Functions::Collections::size(nestedTupleNamesList)-1);
    			}

    			continue;
    		} // End of if(myBlob[idx] == '>')

    		// At this point, we can parse the next available attribute.
    		// e-g: rstring name,
    		// e-g: list<rstring> businesses>
    		// e-g: map<rstring,int32> housingNumbers>
    		// e-g: map<rstring,rstring> officials,
    		// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList,
    		// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList>
    		// We are looking for the type followed by a space followed by the
    		// attribute name followed by either a comma or a close angle bracket.
    		//
    		// (OR)
    		//
    		// In the case of a list of tuple shown above, we have to look for
    		// the type ending with ">> " followed by the attribute name
    		// followed by either a comma or a close angle bracket.
    		//
    		// The list of tuple is obviously more complex. So, let us see
    		// if a given attribute is of that kind first. If not, we will do
    		// the parsing for the other SPL built-in data types described above.
    		int idx2 = -1;

    		if(Functions::String::findFirst(myTupleSchema, "list<tuple<", idx) == idx) {
    			// This is a list<TUPLE>. Let us see if it terminates with ">> ".
    			idx2 = Functions::String::findFirst(myTupleSchema, ">> ", idx);

    			if(idx2 == -1) {
    				// There is no >> at the end of a list of tuple.
    				error = MISSING_TWO_CLOSE_ANGLE_BRACKETS_AFTER_LIST_OF_TUPLE;
    				return(false);
    			}
    		}

    		// We can tolerate the absence of a list<TUPLE> attribute.
    		if(idx2 == -1) {
    			// Look for an other kind of SPL built-in data type which is more common.
    			idx2 = Functions::String::findFirst(myTupleSchema, " ", idx);
    		} else {
    			// We found a list of tuple.
    			// Move idx2 to the location of the space character by
    			// going past the two angle brackets.
    			idx2 += 2;
    		}

    		if(idx2 == -1) {
    			error = MISSING_SPACE_BEFORE_TUPLE_ATTRIBUTE_NAME;
    			return(false);
    		}

    		// Get the tuple attribute type.
    		rstring taType = Functions::String::substring(myTupleSchema, idx, idx2-idx);
    		// Move past the space character now.
    		idx = idx2+1;
    		// Get the tuple attribute name.
    		// The delimiter after the tuple attribute name could be either a
    		// comma or a close angle bracket. We have to consider whichever comes first.
    		idx2 = Functions::String::findFirst(myTupleSchema, ",", idx);
    		int idx3 = Functions::String::findFirst(myTupleSchema, ">", idx);

    		if(idx2 == -1 && idx3 == -1) {
				error = MISSING_COMMA_OR_CLOSE_ANGLE_BRACKET_AFTER_TUPLE_ATTRIBUTE_NAME2;
				return(false);
    		}

    		if(idx2 == -1) {
    			idx2 = idx3;
    		}

    		// If both a comma and a close angle bracket are found anywhere
    		// in the remaining portion of the tuple schema being searched,
    		// then consider whichever comes first after the tuple attribute name.
    		if((idx3 != -1) && (idx3 < idx2)) {
    			idx2 = idx3;
    		}

    		rstring taName = Functions::String::substring(myTupleSchema, idx, idx2-idx);
    		//  Move idx to the position where a comma or a close angle bracket was found.
    		idx = idx2;

    		rstring taNameQualified = "";

    		// We can insert the tuple attribute name and type in the map now.
    		// If this attribute belongs to a nested tuple, we should qualify it properly.
    		for(int i=0; i<Functions::Collections::size(nestedTupleNamesList); i++) {
    			if(taNameQualified != "") {
    				// Add a period.
    				taNameQualified += ".";
    			}

    			taNameQualified += nestedTupleNamesList[i];
    		}

    		// Add the attribute name.
			if(taNameQualified != "") {
				// Add a period.
				taNameQualified += ".";
			}

			taNameQualified += taName;
			// Insert into the map now.
			Functions::Collections::insertM(tupleAttributesMap, taNameQualified, taType);
			continue;
    	} // End of while loop.

    	if(trace == true) {
    		cout << "==== BEGIN eval_predicate trace 4a ====" << endl;
			// Use this for debugging purposes.
			// Display the tuple attributes map.
			SPL::list<rstring> keys = Functions::Collections::keys(tupleAttributesMap);

			for(int i=0; i<Functions::Collections::size(keys); i++) {
				cout << keys[i] << "-->" << tupleAttributesMap[keys[i]] << endl;
			}

			cout << "==== END eval_predicate trace 4a ====" << endl;
    	}

    	return(true);
    } // End of parseTupleAttributes
    // ====================================================================

    // ====================================================================
	// This function traces the inside details about the given tuple.
    // Note: I couldn't make the tupleAttributesMap function argument as a const reference.
    // Because, C++ compiler gives an error saying that the way we index an
    // element from that map in the logic below is not valid on a const referenced map.
    // Hence, I made it as a non-constant reference. So, let us be careful inside this
    // function to use this map only for read and not do any accidental writes into this map.
	inline void traceTupleAtttributeNamesAndValues(Tuple const & myTuple,
		SPL::map<rstring, rstring> & tupleAttributesMap, boolean trace) {
		// This function will not do any work if tracing is turned off.
    	if(trace == false) {
    		return;
    	}

		// Example of myTuple's schema:
		// myTuple=tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>
		//

		// I used this block of code simply for testing the
		// following Tuple class member function.
		tr1::unordered_map<string, uint32_t> tupleAttributesMap2;
		tupleAttributesMap2 = myTuple.getAttributeNames();

		cout << "==== BEGIN eval_predicate trace 5a ====" << endl;
		cout << "tupleAttributesMap2=";
		for (std::tr1::unordered_map<string, uint32_t>::const_iterator it = tupleAttributesMap2.begin();
			it != tupleAttributesMap2.end(); ++it) {
			  cout << " " << it->first << ":" << it->second;
		}

		cout << endl;

		// Let us now look deep inside of this tuple and trace its
		// attribute names and values.
    	// Get the keys of our map into a list. These keys represent the
    	// fully qualified attribute names present in the user provided tuple.
    	SPL::list<rstring> attribList = Functions::Collections::keys(tupleAttributesMap);

    	// Loop through all the attributes in that tuple and print their values.
    	for (int i=0; i<Functions::Collections::size(attribList); i++) {
    		// We may have attributes in nested tuples. So, let us parse and
    		// get all the nested tuple names that are separated by a period character.
    		SPL::list<rstring> attribTokens =
    			Functions::String::tokenize(attribList[i], ".", false);
    		ConstValueHandle cvh;

    		if(Functions::Collections::size(attribTokens) == 1) {
    			// This is just an attribute that is not in a nested tuple and
    			// instead is part of the top level tuple.
    			cvh = myTuple.getAttributeValue(attribList[i]);
    		} else {
    			// Let us traverse through all the nested tuples and
    			// reach the final tuple in the nested hierarchy for the current attribute.
    			// Start with with the very first tuple i.e. at index 0 in the
    			// list that contains all the nested tuple names.
    			cvh = myTuple.getAttributeValue(attribTokens[0]);

    			// Loop through the list and reach the final tuple in the
    			// nested hierarchy i.e. N-1 where the Nth one is the actual
    			// attribute.
    			for(int j=1; j<Functions::Collections::size(attribTokens)-1; j++) {
    				Tuple const & data1 = cvh;
    				cvh = data1.getAttributeValue(attribTokens[j]);
    			}

    			// Now that we reached the last tuple in the nested hierarchy,
    			// we can read the value of the actual attribute inside that tuple.
    			// e-g: details.location.geo.latitude
    			// In this example, geo is the last tuple in the nested hierarchy and
    			// it contains the latitude attribute.
    			Tuple const & data2 = cvh;
    			cvh = data2.getAttributeValue(attribTokens[Functions::Collections::size(attribTokens)-1]);
    		} // End of else block.

    		/*
    		// You can uncomment this line to debug any type conversion
    		// runtime problems that happen in mapping a given attribute type to ConstValueHandle.
    		// In such problems, it is possible that a careless coding mistake in this function that
    		// tries to assign a wrong data type to a ConstValueHandle on the RHS.
    		// Following console print will reveal the attribute name and the attribute type.
    		// Then, search for that attribute type in the code below to confirm that the
    		// assignment to ConstValueHandle is done for the correct data type.
    		cout << "YYYYY1=" << attribList[i] << ", YYYYY2=" <<
    			tupleAttributesMap[attribList[i]] << endl;
    		*/

    		// We can now fetch and trace the name/value of the given attribute.
    		if(tupleAttributesMap[attribList[i]] == "rstring") {
    			rstring const & myString = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myString << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "int32") {
    			int32 const & myInt32 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myInt32 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "uint32") {
    			uint32 const & myUInt32 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myUInt32 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "int64") {
    			int64 const & myInt64 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myInt64 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "uint64") {
    			uint64 const & myUInt64 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myUInt64 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "float32") {
    			float32 const & myFloat32 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myFloat32 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "float64") {
    			float64 const & myFloat64 = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myFloat64 << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "boolean") {
    			boolean const & myBoolean = cvh;
    			cout << "name=" << attribList[i] << ", value=" << myBoolean << endl;
    		} else if(tupleAttributesMap[attribList[i]] == "set<int32>") {
    			SPL::set<int32> const & mySetInt32 = cvh;
    			ConstSetIterator it = mySetInt32.getBeginIterator();

    			while (it != mySetInt32.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				int32 const & myInt32 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myInt32 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "set<int64>") {
    			SPL::set<int64> const & mySetInt64 = cvh;
    			ConstSetIterator it = mySetInt64.getBeginIterator();

    			while (it != mySetInt64.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				int64 const & myInt64 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myInt64 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "set<float32>") {
    			SPL::set<float32> const & mySetFloat32 = cvh;
    			ConstSetIterator it = mySetFloat32.getBeginIterator();

    			while (it != mySetFloat32.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				float32 const & myFloat32 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myFloat32 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "set<float64>") {
    			SPL::set<float64> const & mySetFloat64 = cvh;
    			ConstSetIterator it = mySetFloat64.getBeginIterator();

    			while (it != mySetFloat64.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				float64 const & myFloat64 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myFloat64 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "set<rstring>") {
    			SPL::set<rstring> const & mySetRString = cvh;
    			ConstSetIterator it = mySetRString.getBeginIterator();

    			while (it != mySetRString.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				rstring const & myRString = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myRString << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "list<int32>") {
    			SPL::list<int32> const & myListInt32 = cvh;
    			ConstListIterator it = myListInt32.getBeginIterator();

    			while (it != myListInt32.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				int32 const & myInt32 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myInt32 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "list<int64>") {
    			SPL::list<int64> const & myListInt64 = cvh;
    			ConstListIterator it = myListInt64.getBeginIterator();

    			while (it != myListInt64.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				int64 const & myInt64 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myInt64 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "list<float32>") {
    			SPL::list<float32> const & myListFloat32 = cvh;
    			ConstListIterator it = myListFloat32.getBeginIterator();

    			while (it != myListFloat32.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				float32 const & myFloat32 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myFloat32 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "list<float64>") {
    			SPL::list<float64> const & myListFloat64 = cvh;
    			ConstListIterator it = myListFloat64.getBeginIterator();

    			while (it != myListFloat64.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				float64 const & myFloat64 = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myFloat64 << endl;
    				it++;
    			}
    		} else if(tupleAttributesMap[attribList[i]] == "list<rstring>") {
    			SPL::list<rstring> const & myListRString = cvh;
    			ConstListIterator it = myListRString.getBeginIterator();

    			while (it != myListRString.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				rstring const & myRString = myVal;
    				cout << "name=" << attribList[i] << ", value=" << myRString << endl;
    				it++;
    			}
			} else if(tupleAttributesMap[attribList[i]] == "map<rstring,rstring>") {
				SPL::map<rstring,rstring> const & myMapRStringRString = cvh;
				ConstMapIterator it = myMapRStringRString.getBeginIterator();

				while (it != myMapRStringRString.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,rstring> const & myRStringRString = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myRStringRString.first <<
						", " << myRStringRString.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<rstring,int32>") {
				SPL::map<rstring,int32> const & myMapRStringInt32 = cvh;
				ConstMapIterator it = myMapRStringInt32.getBeginIterator();

				while (it != myMapRStringInt32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,int32> const & myRStringInt32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myRStringInt32.first <<
						", " << myRStringInt32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int32,rstring>") {
				SPL::map<int32,rstring> const & myMapInt32RString = cvh;
				ConstMapIterator it = myMapInt32RString.getBeginIterator();

				while (it != myMapInt32RString.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int32,rstring> const & myInt32RString = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt32RString.first <<
						", " << myInt32RString.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<rstring,int64>") {
				SPL::map<rstring,int64> const & myMapRStringInt64 = cvh;
				ConstMapIterator it = myMapRStringInt64.getBeginIterator();

				while (it != myMapRStringInt64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,int64> const & myRStringInt64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myRStringInt64.first <<
						", " << myRStringInt64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int64,rstring>") {
				SPL::map<int64,rstring> const & myMapInt64RString = cvh;
				ConstMapIterator it = myMapInt64RString.getBeginIterator();

				while (it != myMapInt64RString.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int64,rstring> const & myInt64RString = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt64RString.first <<
						", " << myInt64RString.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<rstring,float32>") {
				SPL::map<rstring,float32> const & myMapRStringFloat32 = cvh;
				ConstMapIterator it = myMapRStringFloat32.getBeginIterator();

				while (it != myMapRStringFloat32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,float32> const & myRStringFloat32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myRStringFloat32.first <<
						", " << myRStringFloat32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float32,rstring>") {
				SPL::map<float32,rstring> const & myMapFloat32RString = cvh;
				ConstMapIterator it = myMapFloat32RString.getBeginIterator();

				while (it != myMapFloat32RString.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,rstring> const & myFloat32RString = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat32RString.first <<
						", " << myFloat32RString.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<rstring,float64>") {
				SPL::map<rstring,float64> const & myMapRStringFloat64 = cvh;
				ConstMapIterator it = myMapRStringFloat64.getBeginIterator();

				while (it != myMapRStringFloat64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,float64> const & myRStringFloat64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myRStringFloat64.first <<
						", " << myRStringFloat64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float64,rstring>") {
				SPL::map<float64,rstring> const & myMapFloat64RString = cvh;
				ConstMapIterator it = myMapFloat64RString.getBeginIterator();

				while (it != myMapFloat64RString.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float64,rstring> const & myFloat64RString = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat64RString.first <<
						", " << myFloat64RString.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int32,int32>") {
				SPL::map<int32,int32> const & myMapInt32Int32 = cvh;
				ConstMapIterator it = myMapInt32Int32.getBeginIterator();

				while (it != myMapInt32Int32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int32,int32> const & myInt32Int32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt32Int32.first <<
						", " << myInt32Int32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int32,int64>") {
				SPL::map<int32,int64> const & myMapInt32Int64 = cvh;
				ConstMapIterator it = myMapInt32Int64.getBeginIterator();

				while (it != myMapInt32Int64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int32,int64> const & myInt32Int64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt32Int64.first <<
						", " << myInt32Int64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int64,int32>") {
				SPL::map<int64,int32> const & myMapInt64Int32 = cvh;
				ConstMapIterator it = myMapInt64Int32.getBeginIterator();

				while (it != myMapInt64Int32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int64,int32> const & myInt64Int32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt64Int32.first <<
						", " << myInt64Int32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int64,int64>") {
				SPL::map<int64,int64> const & myMapInt64Int64 = cvh;
				ConstMapIterator it = myMapInt64Int64.getBeginIterator();

				while (it != myMapInt64Int64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int64,int64> const & myInt64Int64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt64Int64.first <<
						", " << myInt64Int64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int32,float32>") {
				SPL::map<int32,float32> const & myMapInt32Float32 = cvh;
				ConstMapIterator it = myMapInt32Float32.getBeginIterator();

				while (it != myMapInt32Float32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int32,float32> const & myInt32Float32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt32Float32.first <<
						", " << myInt32Float32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int32,float64>") {
				SPL::map<int32,float64> const & myMapInt32Float64 = cvh;
				ConstMapIterator it = myMapInt32Float64.getBeginIterator();

				while (it != myMapInt32Float64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int32,float64> const & myInt32Float64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt32Float64.first <<
						", " << myInt32Float64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int64,float32>") {
				SPL::map<int64,float32> const & myMapInt64Float32 = cvh;
				ConstMapIterator it = myMapInt64Float32.getBeginIterator();

				while (it != myMapInt64Float32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int64,float32> const & myInt64Float32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt64Float32.first <<
						", " << myInt64Float32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<int64,float64>") {
				SPL::map<int64,float64> const & myMapInt64Float64 = cvh;
				ConstMapIterator it = myMapInt64Float64.getBeginIterator();

				while (it != myMapInt64Float64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<int64,float64> const & myInt64Float64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myInt64Float64.first <<
						", " << myInt64Float64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float32,int32>") {
				SPL::map<float32,int32> const & myMapFloat32Int32 = cvh;
				ConstMapIterator it = myMapFloat32Int32.getBeginIterator();

				while (it != myMapFloat32Int32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,int32> const & myFloat32Int32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat32Int32.first <<
						", " << myFloat32Int32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float32,int64>") {
				SPL::map<float32,int64> const & myMapFloat32Int64 = cvh;
				ConstMapIterator it = myMapFloat32Int64.getBeginIterator();

				while (it != myMapFloat32Int64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,int64> const & myFloat32Int64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat32Int64.first <<
						", " << myFloat32Int64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float64,int32>") {
				SPL::map<float64,int32> const & myMapFloat64Int32 = cvh;
				ConstMapIterator it = myMapFloat64Int32.getBeginIterator();

				while (it != myMapFloat64Int32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float64,int32> const & myFloat64Int32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat64Int32.first <<
						", " << myFloat64Int32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float64,int64>") {
				SPL::map<float64,int64> const & myMapFloat64Int64 = cvh;
				ConstMapIterator it = myMapFloat64Int64.getBeginIterator();

				while (it != myMapFloat64Int64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float64,int64> const & myFloat64Int64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat64Int64.first <<
						", " << myFloat64Int64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float32,float32>") {
				SPL::map<float32,float32> const & myMapFloat32Float32 = cvh;
				ConstMapIterator it = myMapFloat32Float32.getBeginIterator();

				while (it != myMapFloat32Float32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,float32> const & myFloat32Float32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat32Float32.first <<
						", " << myFloat32Float32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float32,float64>") {
				SPL::map<float32,float64> const & myMapFloat32Float64 = cvh;
				ConstMapIterator it = myMapFloat32Float64.getBeginIterator();

				while (it != myMapFloat32Float64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,float64> const & myFloat32Float64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat32Float64.first <<
						", " << myFloat32Float64.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float64,float32>") {
				SPL::map<float64,float32> const & myMapFloat64Float32 = cvh;
				ConstMapIterator it = myMapFloat64Float32.getBeginIterator();

				while (it != myMapFloat64Float32.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float64,float32> const & myFloat64Float32 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat64Float32.first <<
						", " << myFloat64Float32.second << endl;
					it++;
				}
			} else if(tupleAttributesMap[attribList[i]] == "map<float64,float64>") {
				SPL::map<float64,float64> const & myMapFloat64Float64 = cvh;
				ConstMapIterator it = myMapFloat64Float64.getBeginIterator();

				while (it != myMapFloat64Float64.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float64,float64> const & myFloat64Float64 = myVal;
					cout << "name=" << attribList[i] <<
						", value=" <<  myFloat64Float64.first <<
						", " << myFloat64Float64.second << endl;
					it++;
				}
			} else if(Functions::String::findFirst(
				tupleAttributesMap[attribList[i]], "list<tuple<") == 0) {
				// We have a list of tuple. This list doesn't hold
				// items that are made of well defined SPL built-in data types.
				// Instead, it holds a user defined Tuple type. So, we can't
				// adopt the technique we used in the other else blocks above
				// to assign cvh to a variable such as SPL::list<rstring>.
				// We must use the C++ interface type SPL::List that the
				// SPL::list is based on.
				// The line below shows an example of an attribute type (schema)
				// followed by an attribute name.
				// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList
    			SPL::List const & myListTuple = cvh;
    			ConstListIterator it = myListTuple.getBeginIterator();

				// Use of "lot" in the local variable names below means List Of Tuple.
    			while (it != myListTuple.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				Tuple const & lotTuple = myVal;

    				// We can now get the attributes present inside this tuple.
    				//
    				int32 lotSchemaLength =
    					Functions::String::length(tupleAttributesMap[attribList[i]]);

    				// Get just the tuple<...> part from this attribute's type.
    				// We have to skip the initial "list<" portion in the type string.
    				// We also have to skip the final > in the type string that
    				// is a closure for "list<" i.e. we start from the
    				// zero based index 5 and take the substring until the
    				// end except for the final > which in total is 6 characters
    				// less than the length of the entire type string.
    				rstring lotTupleSchema =
    					Functions::String::substring(
    					tupleAttributesMap[attribList[i]],
						5, lotSchemaLength-6);
    				SPL::map<rstring, rstring> lotTupleAttributesMap;
    				int32 lotError = 0;
    				boolean lotResult = parseTupleAttributes(lotTupleSchema,
    					lotTupleAttributesMap, lotError, trace);

    				if(lotResult == false) {
    					cout << "It failed to get the list<TUPLE> attributes for " <<
    						attribList[i] << ". Error=" << lotError <<
							". Tuple schema=" << lotTupleSchema << endl;
    				} else {
    					// We got the LOT tuple attributes.
    					// We can recursively call the current
    					// method that we are in now to trace the tuple attributes.
    					cout << "BEGIN Recursive trace tuple attributes call for list<TUPLE> " <<
    						attribList[i] << "." << endl;
    					traceTupleAtttributeNamesAndValues(lotTuple,
    						lotTupleAttributesMap, trace);
    					cout << "END Recursive trace tuple attributes call for list<TUPLE> " <<
    						attribList[i] << "." << endl;
    				}

    				it++;
    			}
			} else {
				cout << "Skipping the trace for an unsupported attribute " <<
					"type in the tuple: Attribute Name=" <<
					attribList[i] << ", Attribute Type=" <<
					tupleAttributesMap[attribList[i]] << endl;
			}
    	} // End of the for loop.

		cout << "==== END eval_predicate trace 5a ====" << endl;
	} // End of traceTupleAtttributeNamesAndValues
	// ====================================================================

	// ====================================================================
    // This function validates the user given expression.
	// It validates if the attributes referred in the expression really
	// exist in the user given tuple. During this process, it also
	// builds a structure of how the expression is composed by recording
	// the usage of attributes (LHS), operations performed (in the middle),
	// the values to be compared (RHS) and the intra subexpression logical
	// operators if any. By building that structure once here,
	// it can be reused when the user application ode calls the eval_predicate
	// function available in this file repeatedly for evaluating a
	// given expression. It will help in improving the expression evaluation performance.
	// This subexpression layout list will have a sequence of items as shown below.
	// LHSAttribName
	// LHSAttribType
	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
	// RHSValue
	// Intra subexpression logical operator - When N/A, it will have an empty string.
	// ...   - The sequence above repeats for this subexpression.
	//
	// In addition to this structure, this function also prepares another list for
	// storing all the inter subexpression logical operators.
	//
	// e-g:
	// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
	// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
	//
	// Note: The space below between > > is a must. Otherwise, compiler will give an error.
    inline boolean validateExpression(rstring const & expr,
        SPL::map<rstring, rstring> const & tupleAttributesMap,
    	SPL::map<rstring, SPL::list<rstring> > & subexpressionsMap,
		SPL::map<rstring, rstring> & intraNestedSubexpressionLogicalOperatorsMap,
    	SPL::list<rstring> & interSubexpressionLogicalOperatorsList,
    	int32 & error, int32 & validationStartIdx, boolean trace=false) {
    	error = ALL_CLEAR;

    	// This method will do its work and keep populating the
    	// subexpressions layout map and the inter subexpression logical
    	// operator list which were passed as input reference arguments.
    	//
    	// This method will get called recursively when a list<TUPLE> is
    	// encountered in a given expression. In that case, the validationStartIdx
    	// method argument will tell us where to start the validation from.
    	// In a non-recursive call into this method, validationStartIdx is
    	// set to 0 to start the validation from the beginning of the expression.
    	// It is important to note that the recursive caller must always pass
    	// its own newly formed local variables as method arguments here so as
    	// not to overwrite the original reference pointers passed by the
    	// very first caller who made the initial non-recursive call.
    	//
    	// When evaluating a subexpression involving a list<TUPLE>, our
    	// evaluation method available later in this file will also call this
    	// validation method in a non-recursive manner. Since it has to
    	// evaluate only that single subexpression, it will only send that
    	// single subexpression string on its own starting at index 0.
    	// That method will set the validationStartIdx to 0. That is perfectly
    	// fine for doing an one shot single subexpression validation.
    	// The main purpose there is to build the subexpression layout list for
    	// that subexpression involving a list<TUPLE> as that is a
    	// key component for doing the evaluation.
    	//
    	// So, there is nothing to be concerned about the way this method will get
    	// called in a few special ways as explained above in those two paragraphs.
    	//
    	// Get a blob representation of the expression.
    	SPL::blob myBlob = Functions::String::convertToBlob(expr);
    	int32 stringLength = Functions::String::length(expr);
    	uint8 currentChar = 0, previousChar = 0;
    	stack <uint8> st;
    	boolean openQuoteCharacterFound = false;

    	// At first, let us match all the parentheses and the square brackets.
    	for(int i=0; i<stringLength; i++) {
        	// We can skip this check if this method is being called recursively.
        	// Because during the very first non-recursive call, this
        	// parenthesis matching logic would have already been performed.
    		// There is no point in doing it during a recursive call which will
    		// force the validation to start from an arbitrary position in the
    		// expression that is not conducive for this check.
    		if(validationStartIdx > 0) {
    			break;
    		}

    		currentChar = myBlob[i];
    		// We can only allow certain characters in the expression.
    		// It should be any character from ASCII 20 to 126.
    		if(currentChar < ' ' || currentChar > '~') {
    			error = INVALID_CHARACTER_FOUND_IN_EXPRESSION;
    			return(false);
    		}

    		// We will permit the presence of open and close
    		// characters within a quoted string value that
    		// is part of a given expression string.
    		// In order to do that, we must keep track of
    		// quote characters to help us in knowing whether
    		// we are within a string value or not at any given time.
    		// This logic helps us in skipping the parenthesis and
    		// bracket matching when those characters appear within
    		// an LHS and/or RHS string in a given expression.
    		//
    		// Example of such an expression string:
    		// '(Body.MachineStatus.Status["K(\'e]y)2"] == "V[a\'l)u(e[2")'
    		//
    		if(openQuoteCharacterFound == false &&
    			(currentChar == '"' || currentChar == '\'')) {
    			// This is a new open quote character we are seeing.
    			// It could either be in LHS as part of a map key string or
    			// in RHS as part of a string value.
    			openQuoteCharacterFound = true;
    			continue;
    		}

    		if(openQuoteCharacterFound == true &&
    			(currentChar == '"' || currentChar == '\'')) {
    			// We have to now ensure if it is an embedded
    			// quote character within a string or it is a
    			// quote character indicating the end of a
    			// given string which could be an LHS map key
    			// string or an RHS string value.
    			//
    			// Check if it indicates the end of a map key string.
    			if(isQuoteCharacterAtEndOfMapKeyString(myBlob, i) == true) {
    				// Reset this flag.
    				openQuoteCharacterFound = false;
        		// Check if it indicates the end of an RHS string value.
    			} else if(isQuoteCharacterAtEndOfRhsString(myBlob, i) == true) {
    				// Reset this flag.
    				openQuoteCharacterFound = false;
    			}

    			continue;
    		}

    		if(currentChar != '(' && currentChar != '[' &&
    			currentChar != ')' && currentChar != ']') {
    			// These are the regular characters for which we have
    			// no need for pushing and popping in our stack.
    			continue;
    		}

    		// If we are currently within a LHS/RHS string
    		// component of a given expression string,
    		// we will allow any embedded open and close
    		// characters without pushing and popping them in
    		// our stack used for parenthesis and bracket matching.
    		if(openQuoteCharacterFound == true) {
    			// This means, we are within a string component.
    			// We will allow free form usage of these
    			// open and close characters within a string.
    			// No need to get them involved in our strict
    			// parenthesis and bracket matching logic below.
    			continue;
    		}

    		// As we encounter the allowed open characters, push them into the stack.
    		if(currentChar == '(' || currentChar == '[') {
    			st.push(myBlob[i]);
    			continue;
    		}

    		// If it is an allowed close character, check if we have the
    		// corresponding open character at the top of the stack.
    		if((currentChar == ')') && st.empty() == true) {
    			error = UNMATCHED_CLOSE_PARENTHESIS_IN_EXPRESSION1;
    			return(false);
    		}

    		if((currentChar == ']') && st.empty() == true) {
    			error = UNMATCHED_CLOSE_BRACKET_IN_EXPRESSION1;
    			return(false);
    		}

    		if(currentChar == ')' && st.top() != '(') {
    			error = UNMATCHED_CLOSE_PARENTHESIS_IN_EXPRESSION2;
    			return(false);
    		}

    		if(currentChar == ')' && st.top() == '(') {
    			st.pop();
    			continue;
    		}

    		if(currentChar == ']' && st.top() != '[') {
    			error = UNMATCHED_CLOSE_BRACKET_IN_EXPRESSION2;
    			return(false);
    		}

    		if(currentChar == ']' && st.top() == '[') {
    			st.pop();
    			continue;
    		}
    	} // End of for loop.

    	// If we still have a non-empty stack after visiting all the
    	// characters in the string, then we have unmatched pair(s).
    	if(st.empty() == false) {
			error = UNMATCHED_OPEN_PARENTHESIS_OR_SQUARE_BRACKET_IN_EXPRESSION;
			return(false);
    	}

    	// At this time, we can validate all the left hand side references to the
    	// tuple attributes, right hand side value comparisons, logical operators etc.
    	// We support these relational operations: ==, !=, <, <=, >, >=
    	// We support these logical operations: ||, &&
    	// We support these arithmetic operations: +, -, *, /, %
    	// Special operations:
    	// contains, startsWith, endsWith, notContains, notStartsWith, notEndsWith, in,
        // containsCI, startsWithCI, endsWithCI, inCI, equalsCI,
    	// notContainsCI, notStartsWithCI, notEndsWithCI, notEqualsCI,
    	// sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT, sizeGE
    	// No bitwise operations are supported at this time.
		rstring relationalAndArithmeticOperations =
			rstring("==,!=,<=,<,>=,>,+,-,*,/,%,") +
			rstring("containsCI,startsWithCI,endsWithCI,inCI,equalsCI,") +
			rstring("notContainsCI,notStartsWithCI,notEndsWithCI,notEqualsCI,") +
			rstring("contains,startsWith,endsWith,") +
			rstring("notContains,notStartsWith,notEndsWith,in,") +
			rstring("sizeEQ,sizeNE,sizeLT,sizeLE,sizeGT,sizeGE");
    	SPL::list<rstring> relationalAndArithmeticOperationsList =
    		Functions::String::csvTokenize(relationalAndArithmeticOperations);

    	rstring logicalOperations = "||,&&";
    	SPL::list<rstring> logicalOperationsList =
    		Functions::String::csvTokenize(logicalOperations);

    	int32 idx = 0;
    	int32 openParenthesisCnt = 0;
    	int32 closeParenthesisCnt = 0;
    	boolean lhsFound = false;
    	boolean lhsSubscriptForListAndMapAdded = false;
    	boolean operationVerbFound = false;
    	boolean rhsFound = false;
    	boolean logicalOperatorFound = false;
    	rstring currentOperationVerb = "";
    	rstring mostRecentLogicalOperatorFound = "";
    	// It tells the number of parts in a multi-part subexpression.
    	int32 multiPartSubexpressionPartsCnt = 0;
    	//
    	// Please note that this function receives a reference to a
    	// map as an argument where it will keep storing all the
    	// subexpressions we find. That map is an important one which
    	// acts as a structure describing the composition of the
    	// entire expression. Its structure is as explained here.
    	// This map's key is a subexpression id.
    	// Subexpression id will go something like this:
    	// 1.1, 1.2, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.1, 4.2, 4.3, 5.1
    	// Subexpression id is made of level 1 and level 2.
		// We support either zero parenthesis or a single level or
		// multilevel (nested) parenthesis.
    	// Logical operators used within a subexpression must be of the same kind.
		//
		// A few examples of zero, single and nested parenthesis.
		//
		// 1.1             2.1                 3.1           4.1
		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
		//
		// 1.1                               2.1
		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
		//
		// 1.1                2.1                   3.1             4.1
		// (a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)
		//
		// 1.1                          2.1                       2.2
		// (a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)
		//
		// 1.1                 2.1                       2.2
		// (a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))
		//
		// 1.1                                 2.1
		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
		//
		// 1.1                     2.1                 2.2
		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))
		//
    	// 1.1                                        1.2                           2.1
		// ((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)
    	//
    	// 1.1                                                                   1.2                          1.3
    	// (((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')
		//
    	// This map's value holds a list that describes the composition of a
    	// given subexpression. Structure of such a list will go something like this:
    	// This list will have a sequence of rstring items as shown below.
    	// LHSAttribName
    	// LHSAttribType
    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
    	// RHSValue
    	// Intra subexpression logical operator - When N/A, it will have an empty string.
    	// ...   - The sequence above repeats for this subexpression.
    	//
    	// So, we need the following list variable to store the layout of every subexpression.
    	//
    	SPL::list<rstring> subexpressionLayoutList;
    	// We need the following variable to keep the subexpression id levels.
    	// e-g: 1.1, 1.2, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.1, 4.2, 4.3, 5.1
    	rstring subexpressionId = "";
    	int32 currentNestedSubexpressionLevel = 0;
    	boolean lhsPrecededByOpenParenthesis = false;
    	boolean enclosedSingleSubexpressionFound = false;
    	boolean consecutiveCloseParenthesisFound = false;

    	/*
    	*********************************************************
		After several days of doing logic exercises both mentally and
		on paper, I came up with the following high level steps for
		processing the nested subexpressions. In my analysis, it
		worked well for the nested subexpression pattern examples
		that are repeatedly shown in different sections of the
		code in this file. More testing needs to be done using
		additional nested subexpressions to fully validate and
		harden these steps.

		1) This is the logic performed in the Open Parenthesis (OP) processing block.
		a) If the next non-space character is another OP, continue the loop.
		b) If (OP-CP > 1), set currentNestedSubexpressionLevel++.
		c) If subexpressionLayoutList (SELOL) size is zero,
		   set enclosedSingleSubexpressionFound = false and continue the loop.
		d) If SELOL size is non-zero and enclosedSingleSubexpressionFound == true,
		   set enclosedSingleSubexpressionFound = false. Do a special check to
		   see if the current subexpression is self enclosed. If it is,
		   continue the loop.
		e) If SELOL size is non-zero and enclosedSingleSubexpressionFound == false,
		   complete that SELOL by saving its very last logical operator item value
		   into a temporary variable and then setting that item to "".
		f) Insert the SELOL in subexpressionsMap (SEMAP).
		g) Insert the saved logical operator value in intraNestedSubexpressionLogicalOperatorsMap.
		h) Clear SELOL and set multiPartSubexpressionPartsCnt = 0.
		i) Continue the loop.

		2)This is the logic performed in the Close Parenthesis (CP) processing block.
		a) If (OP != CP) and (currentNestedSubexpressionLevel > 0) and
           the next non-space character is another CP, then
           set consecutiveCloseParenthesisFound = true. Continue the loop.

		b) If (OP != CP) and (currentNestedSubexpressionLevel > 0) and
           the next non-space character is not another CP and
           lhsPrecededByOpenParenthesis == true and
           consecutiveCloseParenthesisFound == false, then it is an
           enclosed single subexpression.
           So, set currentNestedSubexpressionLevel = 0 and
           enclosedSingleSubexpressionFound = true. Continue the loop.

		c1) If (OP != CP) and (currentNestedSubexpressionLevel > 0) and
		    the next non-space character is not another CP and
            the complementary condition to (2b) exists and
            SELOL size is not-zero, complete the SELOL by setting
            its very last logical operator item value to "".
		c2) Insert the SELOL in subexpressionsMap (SEMAP).
		c3) Clear SELOL and set multiPartSubexpressionPartsCnt = 0.
		c4) Set enclosedSingleSubexpressionFound = false. Continue the loop.

		d1) If (OP == CP) and (currentNestedSubexpressionLevel > 0),
		    set currentNestedSubexpressionLevel++.
		d2) If SELOL size is non-zero, complete that SELOL by setting its
		    very last logical operator item value to "".
		d3) Insert the SELOL in subexpressionsMap (SEMAP).
		d4) Clear SELOL and set multiPartSubexpressionPartsCnt = 0.
		d5) Set currentNestedSubexpressionLevel = 0.
		d6) Set consecutiveCloseParenthesisFound = false.

		3) This is the logic performed in the Logical Operator processing block.
		a1) If (OP != CP) and (currentNestedSubexpressionLevel > 0 and
		    subexpressionLayoutList (SELOL) size is 0) OR
            (consecutiveCloseParenthesisFound == true), insert the
		    logical operator in the intraNestedSubexpressionLogicalOperatorsMap.
		a2) Set multiPartSubexpressionPartsCnt = 0.
		a3) Set consecutiveCloseParenthesisFound = false.

		b1) If (OP != CP) and (currentNestedSubexpressionLevel > 0) and
		    subexpressionLayoutList (SELOL) size is non-zero, insert the
		    logical operator in the SELOL.
		b2) Increment multiPartSubexpressionPartsCnt.

		c1) If (OP == CP) add the logical operator to the
		    interSubexpressionLogicalOperatorsList.
		c2) If SELOL size is non-zero, complete that SELOL by setting its
		    very last logical operator item value to "".
		c3) Insert the SELOL in subexpressionsMap (SEMAP).
		c4) Clear SELOL and set multiPartSubexpressionPartsCnt = 0.
		c5) Set the currentNestedSubexpressionLevel = 0.
    	*********************************************************
    	*/

    	// If this method is being called recursively, we can
    	// directly go to validate the subexpression to which
    	// the caller must have set the validation start index.
    	if(validationStartIdx > 0) {
    		idx = validationStartIdx;
    	}

    	// Stay in this loop and validate lhs, rhs, logical operators etc. and
    	// keep building the expression evaluation map structure which will be
    	// useful in the next major step in another function.
    	while(idx < stringLength) {
    		if(idx > 0) {
    			previousChar = myBlob[idx-1];
    		}

       		currentChar = myBlob[idx];

    		// If it is a space character, move to the next position.
    		if(currentChar == ' ') {
    			idx++;
    			continue;
    		}

			int32 selolSize =
				Functions::Collections::size(subexpressionLayoutList);
			int32 semapSize =
				Functions::Collections::size(subexpressionsMap);

    		// If it is open parenthesis, do some bookkeeping and move to the next position.
    		// e-g:
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(currentChar == '(') {
    			// We can only allow ( right before an LHS and nowhere else.
    			if(lhsFound != false) {
    				error = OPEN_PARENTHESIS_FOUND_NOT_RIGHT_BEFORE_LHS;
    				return(false);
    			}

    			// A ( can be the very first character of a subexpression.
    			// If not, it must have a space before it or it can
    			// have another ( before in case of nested subexpressions.
    			if(idx > 0 && (myBlob[idx-1] != ' ' && myBlob[idx-1] != '(')) {
    				error = NO_SPACE_OR_ANOTHER_OPEN_PARENTHESIS_BEFORE_NEW_OPEN_PARENTHESIS;
    				return(false);
    			}

    			// We can't allow the first open parenthesis in the middle of the
    			// expression after we already processed at least one subexpression that
    			// had zero parenthesis surrounding it.
    			if(openParenthesisCnt == 0 && (selolSize > 0 || semapSize > 0)) {
    				error = FIRST_OPEN_PARENTHESIS_OCCURS_AFTER_A_COMPLETED_SUBEXPRESSION;
    				return(false);
    			}

    			// This flag is one of the many useful things we have
    			// for processing different sorts of nested subexpressions.
    			// This flag will get reset to false every time when a
    			// logical operator is found after a subexpression.
    			lhsPrecededByOpenParenthesis = true;
    			// As a safety measure, reset this flag.
    			consecutiveCloseParenthesisFound = false;
    			openParenthesisCnt++;

    			// Check if we have to perform the steps needed for
    			// processing the nested subexpressions.
    			// This while loop is used here for a convenience of
    			// avoiding the use of too many if conditional blocks.
    			// So, the last statement in this loop must be a break.
    			while(true) {
					// The next non-space character must not be an
					// open parenthesis for us to continue further.
					if(isNextNonSpaceCharacterOpenParenthesis(myBlob,
						idx, stringLength) == true) {
						break;
					}

					if((openParenthesisCnt - closeParenthesisCnt) <= 1) {
						// We are not in a nested expression.
						break;
					}

					// Increment the nested level by one.
					currentNestedSubexpressionLevel++;

					if(selolSize == 0) {
						// This subexpression processing has to continue before
						// we can continue further steps related to the
						// nested subexpressions.
						// This may turn out to be an enclosed single subexpression when
						// after the full subexpression is parsed. Let us reset this flag now.
						enclosedSingleSubexpressionFound = false;
						break;
					}

					// If we are observing a sequence of enclosed single subexpressions,
					// then we have no further step to perform at this time.
					// e-g: // (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
					if(enclosedSingleSubexpressionFound == true) {
						// Reset this flag now.
						enclosedSingleSubexpressionFound = false;;

						// Do a quick and special look ahead test to see if the
						// current subexpression that we are just about to
						// process is also an enclosed single subexpression.
						// If yes, then there is a sequence of them.
						if(isThisAnEnclosedSingleSubexpression(expr, idx) == true) {
							// Let this enclosed single subexpression chain continue.
							// We have no other step to perform now.
							break;
						}
					}

					// Save the very last logical operator item value in SELOL
					// into a temporary variable
					rstring logicalOperator =
						subexpressionLayoutList[selolSize - 1];
					// We are going to declare the end of this nested subexpression.
					subexpressionLayoutList[selolSize - 1] = "";
					// Get a new id for this nested subexpression.
					// e-g: 2.3
					// Please note that we want a new id for the previously
					// parsed and validated nested subexpression.
					// So, remember to reduce the nested level by one to
					// refer to the subexpression that just got processed.
					getNextSubexpressionId(currentNestedSubexpressionLevel - 1,
						subexpressionId);
					// Insert the SELOL into the SEMAP now using the new id.
					Functions::Collections::insertM(subexpressionsMap,
						subexpressionId, subexpressionLayoutList);
					// Insert the saved logical operator value into its own map.
					Functions::Collections::insertM(intraNestedSubexpressionLogicalOperatorsMap,
						subexpressionId, logicalOperator);
					// Reset the ones below.
					Functions::Collections::clearM(subexpressionLayoutList);
					multiPartSubexpressionPartsCnt = 0;
					// We must be in this loop only for one iteration.
					break;
    			} // End of while(true)

    			idx++;
    			continue;
    		} // End of if(currentChar == '(')

    		// If it is a close parenthesis, let us see if it appears in the expected place.
    		// e-g:
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(currentChar == ')') {
    			// A close parenthesis can occur only if there is a pending open parenthesis.
    			if(openParenthesisCnt == 0) {
    				// This condition must have already been detected above in
    				// the parenthesis matching check we did at the top of this function.
    				// This is here just as an additional safety measure.
    				error = CLOSE_PARENTHESIS_FOUND_WITH_ZERO_PENDING_OPEN_PARENTHESIS;
    				return(false);
    			}

    			// We can only allow ) right after an RHS and nowhere else.
    			if(rhsFound != true) {
    				error = CLOSE_PARENTHESIS_FOUND_NOT_RIGHT_AFTER_RHS;
    				return(false);
    			}

    			// A ) can be the very last character of the expression.
    			// If not, it must have a space after it or it can
    			// have another ) before in case of nested subexpressions.
    			if((idx < stringLength-1) && (myBlob[idx+1] != ' ' && myBlob[idx+1] != ')')) {
    				error = NO_SPACE_OR_ANOTHER_CLOSE_PARENTHESIS_AFTER_NEW_CLOSE_PARENTHESIS;
    				return(false);
    			}

    			closeParenthesisCnt++;

    			// Check if we have to perform the steps needed for
    			// processing the nested subexpressions.
    			// This while loop is used here for a convenience of
    			// avoiding the use of too many if conditional blocks.
    			// So, the last statement in this loop must be a break.
    			while(true) {
    				if(currentNestedSubexpressionLevel == 0) {
    					// We are not in a nested expression.
    					break;
    				}

    				if(openParenthesisCnt != closeParenthesisCnt) {
    					// The next non-space character must not be a
    					// close parenthesis for us to continue further.
    					if(isNextNonSpaceCharacterCloseParenthesis(myBlob,
    						idx, stringLength) == true) {
    						// We see here consecutive occurrences of CP.
    						// It means a nested subexpression has completed.
    						// We will do the actual processing during the
    						// next iteration of the main outer while loop.
    						// This flag will get reset either in the
    						// (OP == CP) processing block below or
    						// later in the logical operator processing block or
    						// the next time when an OP is found and processed.
    						consecutiveCloseParenthesisFound = true;
    						break;
    					}

    					if(selolSize == 0) {
    						// This should never happen.
    						// i.e. a CP with no entries in SELOL.
    						// But, we will still check for it.
    						break;
    					}

    					if(lhsPrecededByOpenParenthesis == true &&
    						consecutiveCloseParenthesisFound == false) {
    						// A presence of CP like this means that the
    						// SELOL doesn't have the Intra subexpression
    						// logical operator entry yet. The last item in
    						// SELOL probably is the RHSValue. And then we
    						// are now seeing this CP. It indicates that it is
    						// an enclosed single subexpression.
    						// Reset the nested subexpression level indicator.
    						currentNestedSubexpressionLevel = 0;
    						// Set this flag to say that it is an enclosed single SE.
    						enclosedSingleSubexpressionFound = true;
    						break;
    					} else {
    						// This CP is not happening for a single SE enclosure.
    						// Instead, it is happening at the end of a
    						// nested subexpression.
        					// If SELOL is not empty, we can complete the current
        					// nested subexpression where this CP is at now.
        					if(selolSize > 0) {
								// Let us append an empty string in the logical operator
								// slot of the subexpression layout list for that last
								// completed nested subexpression.
								rstring str = "";
								Functions::Collections::appendM(subexpressionLayoutList, str);
								// Get a new id for this processed nested subexpression.
								// e-g: 2.3
								int32 nestedLevel = currentNestedSubexpressionLevel;

								// If we have a consecutive CP situation, then we will
								// make the nested level to arbitrary number that is
								// higher than 1 so that it will get the proper
								// nested subexpression id.
								if(consecutiveCloseParenthesisFound == true) {
									// It must be any number higher than 1.
									nestedLevel = 2;
								}

								getNextSubexpressionId(nestedLevel, subexpressionId);
								// Insert the SELOL into the SEMAP now using the new id.
								Functions::Collections::insertM(subexpressionsMap,
									subexpressionId, subexpressionLayoutList);
								// Reset the ones below.
								Functions::Collections::clearM(subexpressionLayoutList);
        					}

							multiPartSubexpressionPartsCnt = 0;
							enclosedSingleSubexpressionFound = false;
							break;
    					}
    				} else {
    					// We have matching OP and CP counts.
    					// Increment the nested subexpression level only
    					// when this CP is not part of an enclosed single SE.
    					if(lhsPrecededByOpenParenthesis != true) {
    						currentNestedSubexpressionLevel++;
    					}

    					// If SELOL is not empty, we can complete the current
    					// nested subexpression where this CP is at now.
    					if(selolSize > 0) {
							// Let us append an empty string in the logical operator
							// slot of the subexpression layout list for that last
							// completed nested subexpression.
							rstring str = "";
							Functions::Collections::appendM(subexpressionLayoutList, str);
							// Get a new id for this processed nested subexpression.
							// e-g: 2.3
							getNextSubexpressionId(currentNestedSubexpressionLevel,
								subexpressionId);
							// Insert the SELOL into the SEMAP now using the new id.
							Functions::Collections::insertM(subexpressionsMap,
								subexpressionId, subexpressionLayoutList);
							// Reset the ones below.
							Functions::Collections::clearM(subexpressionLayoutList);
							multiPartSubexpressionPartsCnt = 0;
    					}

    					multiPartSubexpressionPartsCnt = 0;
    					// Set the nested subexpression level to 0.
    					currentNestedSubexpressionLevel = 0;
    					// Just because OP == CP now, we can reset this flag.
    					consecutiveCloseParenthesisFound = false;
    					break;
    				} // End of the (OP != CP) if block.

					// We must be in this loop only for one iteration.
					break;
    			} // End of while(true)

    			idx++;
    			continue;
    		} // End of if(currentChar == ')')

    		// If we have not yet found the lhs, let us try to match with what is
    		// allowed based on the user given tuple.
    		// e-g:
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(lhsFound == false) {
    			rstring lhsAttribName = "";
    			rstring lhsAttribType = "";
    			lhsSubscriptForListAndMapAdded = false;
				ConstMapIterator it = tupleAttributesMap.getBeginIterator();

				while (it != tupleAttributesMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,rstring> const & myRStringRString = myVal;
					lhsAttribName = myRStringRString.first;
					lhsAttribType = myRStringRString.second;

					int idx2 = Functions::String::findFirst(expr, lhsAttribName, idx);

					if(idx2 == idx) {
						// We must ensure that it is an exact match.
						// That means at the end of this attribute name,
						// we can only have either a space or a [ character or
						// only any of these allowed operation verb characters.
						// ==,!=,<=,<,>=,>,+,-,*,/,%
						int32 lengthOfAttributeName =
							Functions::String::length(lhsAttribName);

						if(idx + lengthOfAttributeName >= stringLength-1) {
							// This LHS attribute name stops abruptly at the
							// very end of the given expression.
							error = LHS_ATTRIB_NAME_STOPS_ABRUPTLY_AT_THE_END_OF_THE_EXPRESSION;
							return(false);
						}

						if((idx + lengthOfAttributeName) < stringLength &&
							myBlob[idx + lengthOfAttributeName] != ' ' &&
							myBlob[idx + lengthOfAttributeName] != '[' &&
							myBlob[idx + lengthOfAttributeName] != '=' &&
							myBlob[idx + lengthOfAttributeName] != '!' &&
							myBlob[idx + lengthOfAttributeName] != '<' &&
							myBlob[idx + lengthOfAttributeName] != '>' &&
							myBlob[idx + lengthOfAttributeName] != '+' &&
							myBlob[idx + lengthOfAttributeName] != '-' &&
							myBlob[idx + lengthOfAttributeName] != '*' &&
							myBlob[idx + lengthOfAttributeName] != '/' &&
							myBlob[idx + lengthOfAttributeName] != '%') {
							// This means, we only had a partial attribute name match.
							// Let us continue the loop and look for finding an
							// exact match with some other attribute name in the given tuple.
							//
							// This line is here to debug when an LHS gets
							// matched partially with a tuple attribute name.
							if(trace == true) {
								cout << "_XXXXX_ ^" <<
									myBlob[idx + lengthOfAttributeName] <<
									"^ AttribName=" << lhsAttribName << endl;
							}

							// Increment the map iterator before continuing the loop.
							it++;
							continue;
						}

						// We have a good start in finding a new LHS.
						// Before we do that, we can reset these status flags that
						// may have been set in the previous processing step.
						// It is done simply as a safety measure.
						logicalOperatorFound = false;
						enclosedSingleSubexpressionFound = false;

						// Let us ensure that we meet one of these parenthesis rules before
						// we can proceed with processing this LHS.
						// 1) If zero parenthesis got used thus far in the
						//    overall expression, we are good.
						//
						// 2) If there are more open parenthesis got used than the
						//    close parenthesis thus far, that means this LHS is
						//    most likely part of a valid subexpression. So, we are good.
						//
						// 3) If non-zero parenthesis got used thus far and the
						//    number of open and close parenthesis stay at the same
						//    count, then that will lead to an incorrect expression syntax.
						//    So, we are not good with this condition.
						//
						if(openParenthesisCnt > 0 &&
							openParenthesisCnt == closeParenthesisCnt) {
							// We are not going to support a condition where the use of
							// parenthesis is not used consistently throughout the expression.
							// e-g:  (symbol startsWith 'INTC') && price <= 97.34 && buyOrSell == true
							error = PARENTHESIS_NOT_USED_CONSISTENTLY_THROUGHOUT_THE_EXPRESSION;
							return(false);
						}

						// Add the lhs tuple attribute name and
						// its type to the subexpression layout list.
				    	// This list will have a sequence of rstring items as shown below.
				    	// LHSAttribName
				    	// LHSAttribType
				    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
				    	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
				    	// RHSValue
				    	// Intra subexpression logical operator - When N/A, it will have an empty string.
				    	// ...   - The sequence above repeats for this subexpression.
				    	//						//
						// e-g:
						// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
						// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
						Functions::Collections::appendM(subexpressionLayoutList,
							lhsAttribName);
						Functions::Collections::appendM(subexpressionLayoutList,
							lhsAttribType);
						// If it is list or map, we must ensure that the next
						// character sequence is [3] for list and
						// ["xyz"] or [123] or [12.34]  for map.
						// Move the idx past the end of the matching attribute name.
						idx += Functions::String::length(lhsAttribName);

						// If we have a list, it can be for list of
						// SPL built-in data types or for list<TUPLE>.
						// We will process both of them here.
						// If it is a list<TUPLE>, then we will do additional
						// validation of the attribute access inside of
						// that tuple in a different code block outside of
						// this if block.
						if(Functions::String::findFirst(lhsAttribType, "list") == 0) {
							// We must ensure that it has [n] following it.
							// The next character we find must be open square bracket [.
							boolean openSquareBracketFound = false;

							while(idx < stringLength) {
								// Skip any space characters preceding the [ character.
								if(myBlob[idx] == ' ') {
									idx++;
									continue;
								} else if(myBlob[idx] == '[') {
									openSquareBracketFound = true;
									break;
								} else {
									// A non-space or non-[ character found which is
									// valid when the operation verb following this LHS is
									// either contains or notContains or containsCI or
									// notContainsCI or sizeXX.
									// If not, it is not valid. We will do this check
									// right after this while loop.
									break;
								}

								idx++;
							} // End of inner while loop.

							if(openSquareBracketFound == false) {
								boolean noOpenSquareBracketAllowed = false;

								// If it is a list<TUPLE> attribute with no [ character,
								// it is valid only if the following operation verb is a sizeXX.
								if(Functions::String::findFirst(lhsAttribType, "list<tuple<" ) == 0 &&
									(Functions::String::findFirst(expr, "sizeEQ", idx) == idx ||
									Functions::String::findFirst(expr, "sizeNE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGE", idx) == idx)) {
									noOpenSquareBracketAllowed = true;
								}

								if(Functions::String::findFirst(lhsAttribType, "list<tuple<" ) == 0 &&
									noOpenSquareBracketAllowed == false) {
									// The check we did in the previous if block didn't pass.
									// In that case, no open square bracket condition is not allowed.
									error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST_OF_TUPLE;
									return(false);
								}

								// A list attribute having a built-in SPL data type with
								// no [ is valid only if the following operation verb is
								// either contains or notContains or sizeXX.
								if((Functions::String::findFirst(lhsAttribType, "list<tuple<") == -1) &&
									(Functions::String::findFirst(expr, "contains", idx) == idx ||
									Functions::String::findFirst(expr, "notContains", idx) == idx ||
									Functions::String::findFirst(expr, "containsCI", idx) == idx ||
									Functions::String::findFirst(expr, "notContainsCI", idx) == idx ||
									Functions::String::findFirst(expr, "sizeEQ", idx) == idx ||
									Functions::String::findFirst(expr, "sizeNE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGE", idx) == idx)) {
									noOpenSquareBracketAllowed = true;
								}

								if(noOpenSquareBracketAllowed == true) {
									// This is allowed. We can break out of the while loop that is
									// iterating over the tuple attributes map.
									// We have no list index value in this case.
									rstring str = "";
									Functions::Collections::appendM(subexpressionLayoutList, str);
									// We completed the validation of the lhs.
									lhsFound = true;
									// We can break out of the outer while loop to
									// continue processing the operation verb.
									break;
								} else {
									// In this case, missing [ is not valid.
									error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST;
									return(false);
								}
							}

							// After the open square bracket, we can have a
							// number as the list index.
							// Move past the open square bracket.
							idx++;
							boolean allNumeralsFound = false;
							boolean closeSquareBracketFound = false;
							boolean spaceFoundAfterListIndexValue = false;
							rstring listIndexValue =  "";

							// Let us ensure that we see all numerals until we see a
							// close square bracket ].
							while(idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterListIndexValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the list index value.
									if(listIndexValue != "") {
										spaceFoundAfterListIndexValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// A non-numeral found.
									allNumeralsFound = false;
									break;
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterListIndexValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									listIndexValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop.

							if(spaceFoundAfterListIndexValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_LIST_INDEX;
								return(false);
							}

							if(allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_AS_LIST_INDEX;
								return(false);
							}

							if(closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST;
								return(false);
							}

							// This list attribute is now fully validated.
							// Move past the ] character.
							idx++;
							// We can insert the list index into our subexpression layout list.
							Functions::Collections::appendM(subexpressionLayoutList,
								listIndexValue);
							lhsSubscriptForListAndMapAdded = true;
						} // End of if block checking list attribute's [n]

						// If it is a list<TUPLE>, let us now do additional
						// validation for the attribute access within the
						// tuple held inside a given list.
						// e-g: Body.MachineStatus.ComponentList[0].Component["MapKey"] == "MapValue"
						if(Functions::String::findFirst(lhsAttribType, "list<tuple<") == 0) {
							// This must be a . i.e. period character to indicate that a
							// tuple attribute access is being made.
							if(idx < stringLength && myBlob[idx] != '.') {
								// No period found for tuple attribute access.
								error = NO_PERIOD_FOUND_AFTER_LIST_OF_TUPLE;
								return(false);
							}

							// Move past the period character.
							idx++;

							// Use of "lot" in the local variable names below means List Of Tuple.
		    				// We can now get the attributes of the tuple held
							// inside a list<TUPLE>.
		    				//
		    				int32 lotSchemaLength =
		    					Functions::String::length(lhsAttribType);

		    				// Get the tuple<...> part from this attribute's type.
		    				// We have to skip the initial "list<" portion in the type string.
		    				// We also have to skip the final > in the type string that
		    				// is a closure for "list<" i.e. we start from the
		    				// zero based index 5 and take the substring until the
		    				// end except for the final > which in total is 6 characters
		    				// less than the entire type string.
		    				rstring lotTupleSchema =
		    					Functions::String::substring(lhsAttribType,
								5, lotSchemaLength-6);
		    				SPL::map<rstring, rstring> lotTupleAttributesMap;
		    				int32 lotError = 0;
		    				boolean lotResult = parseTupleAttributes(lotTupleSchema,
		    					lotTupleAttributesMap, lotError, trace);

		    				if(lotResult == false) {
		    					// This error should never happen since we
		    					// already passed this check even before
		    					// coming into this validation method.
		    					// We are simply doing it here as a safety measure.
		    					error = ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_VALIDATION;

		    					if(trace == true) {
			    					cout << "It failed to get the list<TUPLE> attributes for " <<
			    						lhsAttribName <<
										" during expression validation. Error=" << lotError <<
										". Tuple schema=" << lotTupleSchema << endl;
		    					}

		    					return(false);
		    				}

							// We got the LOT tuple attributes.
							// We can recursively call the current
							// method that we are in now to validate the
							// tuple attribute access involving a list<TUPLE>.
							if(trace == true) {
								cout << "BEGIN Recursive validate expression call for list<TUPLE> " <<
									lhsAttribName << "." << endl;
							}

							SPL::map<rstring, SPL::list<rstring> > lotSubexpressionsMap;
							SPL::map<rstring, rstring> lotIntraNestedSubexpressionLogicalOperatorsMap;
							SPL::list<rstring> lotInterSubexpressionLogicalOperatorsList;
							// It is a recursive call. So, we can tell this method to
							// start the validation at a specific position in the
							// expression instead of from index 0. It can be done by
							// setting the following variable to a non-zero value.
							validationStartIdx = idx;
							// We will also store the index where the
							// subexpression involving a list<TUPLE> starts for another
							// use at the end of this additional processing we are
							// doing here for a list<TUPLE>.
							int32 lotExpressionStartIdx = idx;
							lotResult = validateExpression(expr, lotTupleAttributesMap,
								lotSubexpressionsMap, lotIntraNestedSubexpressionLogicalOperatorsMap,
								lotInterSubexpressionLogicalOperatorsList, error,
								validationStartIdx, trace);

							if(trace == true) {
								cout << "END Recursive validate expression call for list<TUPLE> " <<
									lhsAttribName << "." << endl;
							}

							if(lotResult == false) {
								// There was a validation error in the recursive call.
								return(false);
							}

							// Now that the recursive call is over, we can set idx to a
							// position up to which the recursive call did the validation.
							idx = validationStartIdx;
							// We must reset it now.
							validationStartIdx = 0;
							// At this point, we correctly validated the
							// subexpression involving a list<TUPLE>.
							// In the recursive call we just returned back from,
							// it has already successfully completed the
							// validation for LHS, OperationVerb and RHS.
							// So, we can set all of them as cleanly completed.
			    			lhsFound = true;
			    			operationVerbFound = true;
			    			rhsFound = true;
			    			//
			    			// LHSAttribName
			    			// LHSAttribType
			    			// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
			    			// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
			    			// RHSValue
			    			// Intra subexpression logical operator - When N/A, it will have an empty string.
			    			// ...   - The sequence above repeats for this subexpression.
			    			//
			    			// Just because this is a list<TUPLE>, we already
			    			// appended a value for the list index entry of
			    			// the subexpression layout list in the previous
			    			// if code block for close bracket detection.
			    			// Now, we can fill the lotExpressionStartIdx in
			    			// the subexpression layout list for
			    			// "operation verb" entry.
			    			// In the "rhs value" entry, we will store the
			    			// current idx position where LOT expression ends.
			    			// These two values will be used later inside
			    			// the evaluation function. This is so important for
			    			// evaluating a subexpression involving a list<TUPLE>.
			    			// Operation verb entry.
					    	ostringstream lotExpStartIdx;
					    	lotExpStartIdx << lotExpressionStartIdx;
					    	rstring myValueString = lotExpStartIdx.str();
							Functions::Collections::appendM(subexpressionLayoutList, myValueString);
					    	// RHS entry.
					    	ostringstream lotExpEndIdx;
					    	lotExpEndIdx << idx;
					    	myValueString = lotExpEndIdx.str();
							Functions::Collections::appendM(subexpressionLayoutList, myValueString);
						} // End of the additional validation for list<TUPLE>

						if(Functions::String::findFirst(lhsAttribType, "map") == 0) {
							// It is a map. We must ensure that it has a valid access scheme following it.
							// We support only very specific things here.
							// e-g: [3], ["xyz"], [45.23]
							// The next character we find must be open square bracket [.
							boolean openSquareBracketFound = false;

							while(idx < stringLength) {
								// Skip any space characters preceding the [ character.
								if(myBlob[idx] == ' ') {
									idx++;
									continue;
								} else if(myBlob[idx] == '[') {
									openSquareBracketFound = true;
									break;
								} else {
									// A non-space or non-[ character found which is
									// valid when the following operation verb is
									// either contains or notContains or containsCI or
									// notContainsCI or sizeXX.
									// If not, it is not valid. We will do this check
									// right after this while loop.
									break;
								}

								idx++;
							} // End of inner while loop.

							if(openSquareBracketFound == false) {
								// A map attribute with no [ is valid only if the
								// following operation verb is either contains or
								// notContains or containsCI or notContainsCI or sizeXX.
								if(Functions::String::findFirst(expr, "contains", idx) == idx ||
									Functions::String::findFirst(expr, "notContains", idx) == idx ||
									Functions::String::findFirst(expr, "containsCI", idx) == idx ||
									Functions::String::findFirst(expr, "notContainsCI", idx) == idx ||
									Functions::String::findFirst(expr, "sizeEQ", idx) == idx ||
									Functions::String::findFirst(expr, "sizeNE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeLE", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGT", idx) == idx ||
									Functions::String::findFirst(expr, "sizeGE", idx) == idx) {
									// This is allowed. We can break out of the while loop that is
									// iterating over the tuple attributes map.
									// We have no map key value in this case.
									rstring str = "";
									Functions::Collections::appendM(subexpressionLayoutList, str);

									// We completed the validation of the lhs.
									lhsFound = true;
									// We can break out of the outer while loop to
									// continue processing the operation verb.
									break;
								} else {
									error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_MAP;
									return(false);
								}
							} // End of if(openSquareBracketFound == false)

							// After the open square bracket, we can have an
							// integer or a float or a string as the map key.
							boolean intMapKey = false;
							boolean floatMapKey = false;
							boolean stringMapKey = false;

							if(Functions::String::findFirst(lhsAttribType, "map<int") == 0) {
								intMapKey = true;
							} else if(Functions::String::findFirst(lhsAttribType, "map<float") == 0) {
								floatMapKey = true;
							} else if(Functions::String::findFirst(lhsAttribType, "map<rstring") == 0) {
								stringMapKey = true;
							} else {
								error = UNSUPPORTED_KEY_TYPE_FOUND_IN_MAP;
								return(false);
							}

							// Move past the open square bracket.
							idx++;
							boolean allNumeralsFound = false;
							int decimalPointCnt = 0;
							boolean openQuoteFound = false;
							boolean closeQuoteFound = false;
							boolean invalidStringCharacterFound = false;
							boolean stringCharacterFoundAfterCloseQuote = false;
							boolean spaceFoundAfterMapValue = false;
							boolean closeSquareBracketFound = false;
							rstring mapKeyValue =  "";

							// Let us ensure that we see all numerals until we see a
							// close square bracket ].
							while(intMapKey == true && idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterMapValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the map key value.
									if(mapKeyValue != "") {
										spaceFoundAfterMapValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// Since it is an integer map key, we can allow
									// negative sign as the first character of the key.
									if(mapKeyValue == "" && myBlob[idx] == '-') {
										// We can allow this.
										mapKeyValue = "-";
									} else {
										// A non-numeral character found.
										allNumeralsFound = false;
										break;
									}
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterMapValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating int map key.

							if(intMapKey == true && spaceFoundAfterMapValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_INT_MAP_KEY;
								return(false);
							}

							if(intMapKey == true && allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_IN_INT_MAP_KEY;
								return(false);
							}

							if(intMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_INT_MAP_KEY;
								return(false);
							}

							// Let us ensure that we see all numerals with a single
							// decimal point until we see a close square bracket ].
							while(floatMapKey == true && idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterMapValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the map key value.
									if(mapKeyValue != "") {
										spaceFoundAfterMapValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// If it is not a numeral, we can only allow
									// only one decimal point either in the beginning or
									// in the middle or in the end of the given numeral value.
									if(myBlob[idx] == '.' ) {
										if(decimalPointCnt < 1) {
											// One decimal point is allowed for a float map key.
											decimalPointCnt++;
											// Add this decimal point to the map key value.
											mapKeyValue += ".";
										} else {
											// We are seeing more than one decimal point.
											// This is not allowed.
											decimalPointCnt++;
											break;
										}
									} else {
										// Since it is a float, we can allow - sign as the
										// first character in the key.
										if(mapKeyValue == "" && myBlob[idx] == '-') {
											mapKeyValue = "-";
										} else {
											// A non-numeral and a non-decimal point found.
											allNumeralsFound = false;
											break;
										}
									}
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterMapValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating float map key.

							if(floatMapKey == true && spaceFoundAfterMapValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && decimalPointCnt == 0) {
								// Missing decimal point in the float map key.
								error = MISSING_DECIMAL_POINT_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && decimalPointCnt > 1) {
								// More than one decimal point in the float map key.
								error = MORE_THAN_ONE_DECIMAL_POINT_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_FLOAT_MAP_KEY;
								return(false);
							}

							// Let us ensure that we see a quoted string until we see a
							// close square bracket ].
							while(stringMapKey == true && idx < stringLength) {
								if(closeQuoteFound == true && myBlob[idx] == ']') {
									// We will allow a close square bracket after a
									// close quote has already been seen.
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ' &&
									(openQuoteFound == false || closeQuoteFound == true)) {
									// We will skip all spaces between [].
									idx++;
									continue;
								} else if(myBlob[idx] == '"' || myBlob[idx] == '\'') {
									if(openQuoteFound == false) {
										openQuoteFound = true;
									} else if(closeQuoteFound == false) {
										// We can have single or double quote characters as
										// part of the map key. So, we can consider this as
										// an end of map key string's close quote only if it
										// appears at the very end of the map key string.
										if(isQuoteCharacterAtEndOfMapKeyString(myBlob, idx) == true) {
											closeQuoteFound = true;
										} else {
											// It is an embedded quote character.
											// We will add it to our map key value.
											mapKeyValue += myBlob[idx];
										}
										// It will continue the while loop after the
										// end of this if conditional code block.
									} else {
										stringCharacterFoundAfterCloseQuote = true;
										break;
									}
								} else if(myBlob[idx] < ' ' || myBlob[idx] > '~') {
									// A non-string character found.
									invalidStringCharacterFound = true;
									break;
								} else {
									// If we already encountered open and close quotes,
									// then we can't allow any other characters as
									// part of this string.
									if(openQuoteFound == true && closeQuoteFound == true) {
										stringCharacterFoundAfterCloseQuote = true;
										break;
									}

									// If we see a map key character before encountering
									// an open quote character, then it is invalid.
									if(openQuoteFound == false) {
										break;
									}

									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating string map key.

							if(stringMapKey == true && openQuoteFound == false) {
								error = MISSING_OPEN_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && closeQuoteFound == false) {
								error = MISSING_CLOSE_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && invalidStringCharacterFound == true) {
								// This is a very rare error.
								error = INVALID_CHAR_FOUND_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && stringCharacterFoundAfterCloseQuote == true) {
								// This is a very rare error.
								error = CHAR_FOUND_AFTER_CLOSE_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_STRING_MAP_KEY;
								return(false);
							}

							// If user gave an empty map key, that is invalid.
							if(mapKeyValue == "") {
								error = EMPTY_STRING_MAP_KEY_FOUND;
								return(false);
							}

							// This map attribute is now fully validated.
							// Move past the ] character.
							idx++;
							// We can insert the list index into our subexpression layout list.
							Functions::Collections::appendM(subexpressionLayoutList,
								mapKeyValue);
							lhsSubscriptForListAndMapAdded = true;
						} // End of if block checking map attribute's [key]

						// We would not have added anything to the subexpression layout list after we added
						// the attribute Name and attributeType if we encountered anything other than a
						// list or map. In that case, we will add an empty string for
						// all the data types other than list and map. Because, they don't use
						// subscript to refer to an lhs value.
						if(lhsSubscriptForListAndMapAdded == false) {
							rstring str = "";
							Functions::Collections::appendM(subexpressionLayoutList, str);
						}

						// We completed the validation of the lhs.
						lhsFound = true;
						// We can break out of  the inner while loop.
						break;
					} // End of the if block that validated the matching tuple attribute.

					// There was no attribute match starting at the current position.
					// Continue to iterate the while loop.
					it++;
				} // End of the inner while loop iterating over the tuple attributes map.

				if(lhsFound == false) {
					error = LHS_NOT_MATCHING_WITH_ANY_TUPLE_ATTRIBUTE;
					return(false);
				}

				if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 6a ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Validation start index=" << validationStartIdx << endl;
					cout <<  "Subexpression layout list after validating an LHS." << endl;
	    			ConstListIterator it = subexpressionLayoutList.getBeginIterator();

	    			while (it != subexpressionLayoutList.getEndIterator()) {
	    				ConstValueHandle myVal = *it;
	    				rstring const & myString = myVal;
	    				cout << myString << endl;
	    				it++;
	    			} // End of list iteration while loop.

	    			cout <<  "Intra nested subexpression logical operators map after validating an LHS." << endl;
					ConstMapIterator it2 = intraNestedSubexpressionLogicalOperatorsMap.getBeginIterator();

					while (it2 != intraNestedSubexpressionLogicalOperatorsMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal2 = *it2;
						std::pair<rstring,rstring> const & myRStringRString = myVal2;
						cout << "NestedSubexpressionId=" << myRStringRString.first <<
							", Logical operator=" <<  myRStringRString.second << endl;
						it2++;
					}

	    			cout <<  "Inter subexpression logical operators list after validating an LHS." << endl;
	    			ConstListIterator it3 = interSubexpressionLogicalOperatorsList.getBeginIterator();

	    			while (it3 != interSubexpressionLogicalOperatorsList.getEndIterator()) {
	    				ConstValueHandle myVal3 = *it3;
	    				rstring const & myString3 = myVal3;
	    				cout << myString3 << endl;
	    				it3++;
	    			}

					cout << "==== END eval_predicate trace 6a ====" << endl;
				} // End of if(trace == true)

				// Continue with the main while loop.
				continue;
    		} // End of if(lhsFound == false)

    		// If we have found an LHS but not an operation verb, we must now
    		// look for one of the supported operators.
    		// i.e. relational or arithmetic or special ones.
    		// ==, !=, <, <=, >, >=
        	// +, -, *, /, %
    		// contains, startsWith, endsWith, notContains, notStartsWith,
    		// notEndsWith, in, containsCI, startsWithCI, endsWithCI, inCI, equalsCI,
    		// notContainsCI, notStartsWithCI, notEndsWithCI, notEqualsCI,
    		// sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT, sizeGE
    		// e-g:
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(lhsFound == true && operationVerbFound == false) {
    			// We have an LHS. So, the next one we must have
    			// is one of the allowed operation verbs.
    			currentOperationVerb = "";
    			ConstListIterator it = relationalAndArithmeticOperationsList.getBeginIterator();

    			while (it != relationalAndArithmeticOperationsList.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				rstring const & myOp = myVal;

    				if(Functions::String::findFirst(expr, myOp, idx) == idx) {
    					// We have an operation verb match.
    					currentOperationVerb = myOp;
    					break;
    				}

    				// Continue to iterate.
    				it++;
    			} // End of list iterator while loop.

    			if(currentOperationVerb == "") {
    				// We don't have a match with a valid operation verb.
    				error = INVALID_OPERATION_VERB_FOUND_IN_EXPRESSION;
    				return(false);
    			}

    			// Let us see if this operation verb is allowed for a given LHS attribute type.
    			//
    			// Attribute type we are interested is at index n-2 as of now.
    	    	// This list will have a sequence of rstring items as shown below.
    	    	// LHSAttribName
    	    	// LHSAttribType
    	    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	    	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
    	    	// RHSValue
    	    	// Intra subexpression logical operator - When N/A, it will have an empty string.
    	    	// ...   - The sequence above repeats for this subexpression.
    	    	//
    			// e-g:
    			// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    			// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    			rstring lhsAttribType = subexpressionLayoutList[
					Functions::Collections::size(subexpressionLayoutList) - 2];

    			// We will allow equivalence or non-equivalence only for
    			// string, int and float based attributes in a non-set data type.
    			if(currentOperationVerb == "==" || currentOperationVerb == "!=") {
    				if(lhsAttribType != "rstring" && lhsAttribType != "int32" &&
    					lhsAttribType != "uint32" && lhsAttribType != "int64" &&
						lhsAttribType != "uint64" && lhsAttribType != "float32" &&
						lhsAttribType != "float64" && lhsAttribType != "boolean" &&
						lhsAttribType != "list<int32>" && lhsAttribType != "list<int64>" &&
						lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
						lhsAttribType != "list<rstring>" &&
						lhsAttribType != "map<rstring,rstring>" &&
						lhsAttribType != "map<rstring,int32>" && lhsAttribType != "map<int32,rstring>" &&
						lhsAttribType != "map<rstring,int64>" && lhsAttribType != "map<int64,rstring>" &&
						lhsAttribType != "map<rstring,float32>" && lhsAttribType != "map<float32,rstring>" &&
						lhsAttribType != "map<rstring,float64>" && lhsAttribType != "map<float64,rstring>" &&
						lhsAttribType != "map<int32,int32>" && lhsAttribType != "map<int32,int64>" &&
						lhsAttribType != "map<int64,int32>" && lhsAttribType != "map<int64,int64>" &&
						lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
						lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
						lhsAttribType != "map<float32,int32>" && lhsAttribType != "map<float32,int64>" &&
						lhsAttribType != "map<float64,int32>" && lhsAttribType != "map<float64,int64>" &&
						lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
						lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>") {
    					// This operator is not allowed for a given LHS attribute type.
    					if(currentOperationVerb == "==") {
    						error = INCOMPATIBLE_DOUBLE_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else {
    						error = INCOMPATIBLE_NOT_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					}

    					return(false);
    				} else {
    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);
    				}
    			} // End of validating the equivalence operator verbs.

    			// We will allow relational operation verbs only for
    			// int, float and string based attributes in a non-set data type.
    			// For a string, we will convert it to an integer and then
    			// perform the given relational operation.
    			if(currentOperationVerb == "<" || currentOperationVerb == "<=" ||
    				currentOperationVerb == ">" || currentOperationVerb == ">=") {
    				if(lhsAttribType != "rstring" && lhsAttribType != "int32" &&
    				lhsAttribType != "uint32" && lhsAttribType != "int64" &&
					lhsAttribType != "uint64" && lhsAttribType != "float32" &&
					lhsAttribType != "float64" &&
					lhsAttribType != "list<int32>" && lhsAttribType != "list<int64>" &&
					lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
					lhsAttribType != "list<rstring>" && lhsAttribType != "map<rstring,rstring>" &&
					lhsAttribType != "map<rstring,int32>" && lhsAttribType != "map<int32,rstring>" &&
					lhsAttribType != "map<rstring,int64>" && lhsAttribType != "map<int64,rstring>" &&
					lhsAttribType != "map<rstring,float32>" && lhsAttribType != "map<float32,rstring>" &&
					lhsAttribType != "map<rstring,float64>" && lhsAttribType != "map<float64,rstring>" &&
					lhsAttribType != "map<int32,int32>" && lhsAttribType != "map<int32,int64>" &&
					lhsAttribType != "map<int64,int32>" && lhsAttribType != "map<int64,int64>" &&
					lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
					lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
					lhsAttribType != "map<float32,int32>" && lhsAttribType != "map<float32,int64>" &&
					lhsAttribType != "map<float64,int32>" && lhsAttribType != "map<float64,int64>" &&
					lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
					lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>") {
    					// This operator is not allowed for a given LHS attribute type.
    					if(currentOperationVerb == "<") {
    						error = INCOMPATIBLE_LESS_THAN_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "<=") {
    						error = INCOMPATIBLE_LESS_THAN_OR_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == ">") {
    						error = INCOMPATIBLE_GREATER_THAN_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else {
    						error = INCOMPATIBLE_GREATER_THAN_OR_EQUALS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					}

    					return(false);
    				} else {
    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);
    				}
    			} // End of validating the relational operator verbs.

    			// We will allow arithmetic operation verbs only for
    			// int and float based attributes in a non-set data type.
    			// e-g:
    			// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    			// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    			if(currentOperationVerb == "+" || currentOperationVerb == "-" ||
    				currentOperationVerb == "*" || currentOperationVerb == "/" ||
					currentOperationVerb == "%") {
    				if(lhsAttribType != "int32" &&
    					lhsAttribType != "uint32" && lhsAttribType != "int64" &&
						lhsAttribType != "uint64" && lhsAttribType != "float32" &&
						lhsAttribType != "float64" &&
						lhsAttribType != "list<int32>" && lhsAttribType != "list<int64>" &&
						lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
						lhsAttribType != "map<rstring,int32>" &&
						lhsAttribType != "map<rstring,int64>" &&
						lhsAttribType != "map<rstring,float32>" &&
						lhsAttribType != "map<rstring,float64>" &&
						lhsAttribType != "map<int32,int32>" && lhsAttribType != "map<int32,int64>" &&
						lhsAttribType != "map<int64,int32>" && lhsAttribType != "map<int64,int64>" &&
						lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
						lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
						lhsAttribType != "map<float32,int32>" && lhsAttribType != "map<float32,int64>" &&
						lhsAttribType != "map<float64,int32>" && lhsAttribType != "map<float64,int64>" &&
						lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
						lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>") {
    					// This operation verb is not allowed for a given LHS attribute type.
    					if(currentOperationVerb == "+") {
    						error = INCOMPATIBLE_ADD_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "-") {
    						error = INCOMPATIBLE_SUBTRACT_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "*") {
    						error = INCOMPATIBLE_MULTIPLY_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "/") {
    						error = INCOMPATIBLE_DIVIDE_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else {
    						error = INCOMPATIBLE_MOD_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					}

    					return(false);
    				} else {
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);

    					// e-g:
    	    			// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    	    			// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    					// For arithmetic operation verbs, we are going to store extra
    					// information. e-g: % 8 ==
    					//
    					// Let us ensure that what follows the arithmetic sign is a number.
    					// It can be an int or float.
    					//
    					boolean allNumeralsFound = false;
    					int32 decimalPointCnt = 0;
    					int32 negativeSignCnt = 0;
    					rstring extraInfo = "";

    					// In this while loop, we will try to parse the numeral value that
    					// appears after the arithmetic sign.
    					while(idx < stringLength) {
							// Skip any space characters preceding the numeral character.
							if(myBlob[idx] == ' ') {
								// We will skip all spaces.
								if(extraInfo != "") {
									// A space appeared after the numeral value.
									// The numeral part ended at the previous position.
									break;
								}

								idx++;
								continue;
							} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
								// If it is a - sign, we can allow only one of it
								// just before the numeral value.
								if(myBlob[idx] == '-' ) {
									if(extraInfo == "") {
										// One negative sign is allowed for an int value.
										negativeSignCnt++;
										// Have a space at the very beginning of the extraInfo.
										extraInfo = " -";
										idx++;
										continue;
									} else {
										// We are seeing a negative sign at a wrong position.
										negativeSignCnt++;
										allNumeralsFound = false;
										break;
									}
								}

								// If it is not a numeral, we can only allow
								// only one decimal point either in the beginning or
								// in the middle or in the end of the given numeral value.
								if(myBlob[idx] == '.' ) {
									if(decimalPointCnt < 1) {
										// One decimal point is allowed for a float value.
										decimalPointCnt++;

										if(extraInfo == "") {
											extraInfo = " ";
										}

										extraInfo += ".";
										idx++;
										continue;
									} else {
										// We are seeing more than one decimal point.
										// This is not allowed.
										decimalPointCnt++;
										break;
									}
								}

								// If we encounter a non-numeral character after we
								// started collecting the numeral value, then it should be okay.
								if(extraInfo != "") {
									// This means that our numeral value ended in the previous position.
									break;
								} else {
									// A non-numeral character found even before we
									// started collecting the numeral value.
									allNumeralsFound = false;
									break;
								}
							} else {
								allNumeralsFound = true;

								if(extraInfo == "") {
									// Start with a space character so that it will
									// appear right after the arithmetic sign.
									extraInfo = " ";
								}

								extraInfo += myBlob[idx];
							}

							idx++;
    					} // End of the inner while loop.

						if(negativeSignCnt > 0 &&
							(lhsAttribType != "int32" && lhsAttribType != "int64" &&
							lhsAttribType != "float32" && lhsAttribType != "float64" &&
							lhsAttribType != "list<int32>" && lhsAttribType != "list<int64>" &&
							lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
							lhsAttribType != "map<rstring,int32>" &&
							lhsAttribType != "map<rstring,int64>" &&
							lhsAttribType != "map<rstring,float32>" &&
							lhsAttribType != "map<rstring,float64>" &&
							lhsAttribType != "map<int32,int32>" && lhsAttribType != "map<int32,int64>" &&
							lhsAttribType != "map<int64,int32>" && lhsAttribType != "map<int64,int64>" &&
							lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
							lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
							lhsAttribType != "map<float32,int32>" && lhsAttribType != "map<float32,int64>" &&
							lhsAttribType != "map<float64,int32>" && lhsAttribType != "map<float64,int64>" &&
							lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
							lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>")) {
							// Let us ensure that we have negative sign only for an int and float attributes on the LHS.
							error = NEGATIVE_SIGN_FOUND_IN_NON_INTEGER_NON_FLOAT_ARITHMETIC_OPERAND;
							return(false);
						}

						if(negativeSignCnt > 0 &&
							Functions::String::findFirst(lhsAttribType, "uint") != -1) {
							// We can't allow negative sign  for an uint attribute on the LHS.
							error = NEGATIVE_SIGN_FOUND_IN_UNSIGNED_INTEGER_ARITHMETIC_OPERAND;
							return(false);
						}

						if(negativeSignCnt > 0 && allNumeralsFound == false) {
							// It could be a condition where the - sign is out of position.
							error = NEGATIVE_SIGN_AT_WRONG_POSITION_IN_ARITHMETIC_OPERAND;
							return(false);
						}

						if(decimalPointCnt > 1) {
							// More than one decimal point in the numeral value.
							error = MORE_THAN_ONE_DECIMAL_POINT_IN_ARITHMETIC_OPERAND;
							return(false);
						}

						if(decimalPointCnt > 0 &&
							(lhsAttribType != "float32" && lhsAttribType != "float64" &&
							lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
							lhsAttribType != "map<rstring,float32>" &&
							lhsAttribType != "map<rstring,float64>" &&
							lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
							lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
							lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
							lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>")) {
							// Let us ensure that we have decimal point only for a float attribute on the LHS.
							error = DECIMAL_POINT_FOUND_IN_NON_FLOAT_ARITHMETIC_OPERAND;
							return(false);
						}

						if(decimalPointCnt == 0 &&
							(lhsAttribType == "float32" || lhsAttribType == "float64" ||
							lhsAttribType == "list<float32>" || lhsAttribType == "list<float64>" ||
							lhsAttribType == "map<rstring,float32>" ||
							lhsAttribType == "map<rstring,float64>" ||
							lhsAttribType == "map<int32,float32>" || lhsAttribType == "map<int32,float64>" ||
							lhsAttribType == "map<int64,float32>" || lhsAttribType == "map<int64,float64>" ||
							lhsAttribType == "map<float32,float32>" || lhsAttribType == "map<float32,float64>" ||
							lhsAttribType == "map<float64,float32>" || lhsAttribType == "map<float64,float64>")) {
							// Let us ensure that a float value has a decimal point.
							error = NO_DECIMAL_POINT_IN_FLOAT_ARITHMETIC_OPERAND;
							return(false);
						}

						if(allNumeralsFound == false) {
							error = ALL_NUMERALS_NOT_FOUND_IN_ARITHMETIC_OPERAND;
							return(false);
						}

						// Now that we have parsed the numeral value in the
						// arithmetic operand, it is time to look for the
						// allowed operation verbs (equivalence and relational) after
						// the arithmetic operand.
						//
						// Consume all spaces appearing after the numeral value.
						while(idx < stringLength) {
							if(myBlob[idx] == ' ') {
								idx++;
								continue;
							} else {
								// A non-space character encountered.
								break;
							}
						}

						if(idx >= stringLength) {
							error = NO_OPERATION_VERB_FOUND_AFTER_ARITHMETIC_OPERAND;
							return(false);
						}

						// Add a space in the extra info.
						extraInfo += " ";

						// Starting from this character, we must see one of the
						// allowed verbs (equivalence and relational).
						if(Functions::String::findFirst(expr, "==", idx) == idx) {
							extraInfo += "==";
							idx += 2;
						} else if(Functions::String::findFirst(expr, "!=", idx) == idx) {
							extraInfo += "!=";
							idx += 2;
						} else if(Functions::String::findFirst(expr, "<=", idx) == idx) {
							extraInfo += "<=";
							idx += 2;
						} else if(Functions::String::findFirst(expr, ">=", idx) == idx) {
							extraInfo += ">=";
							idx += 2;
						} else if(Functions::String::findFirst(expr, "<", idx) == idx) {
							extraInfo += "<";
							idx += 1;
						} else if(Functions::String::findFirst(expr, ">", idx) == idx) {
							extraInfo += ">";
							idx += 1;
						} else {
							error = INVALID_OPERATION_VERB_FOUND_AFTER_ARITHMETIC_OPERAND;
							return(false);
						}

						// We will add the extra info to the current operator verb.
						// It is a very special case done only for arithmetic operations.
						currentOperationVerb += extraInfo;
						// We are done here. Our idx doesn't need any further
						// adjustment as it is already pointing past the
						// operation verb we matched above.
    				} // End of else block.
    			} // End of validating arithmetic operator verbs.

    			// We will allow contains, notContains, containsCI, notContainsCI and sizeXX only for
    			// string, set, list and map based attributes.
				// For maps, the first two of these verbs are applicable to
    			// check whether a map contains or notContains a given key.
    			// For all other data types, existence check is done for the
    			// contents stored inside that collection.
    			if(currentOperationVerb == "contains" ||
    				currentOperationVerb == "notContains" ||
					currentOperationVerb == "containsCI" ||
					currentOperationVerb == "notContainsCI" ||
					currentOperationVerb == "sizeEQ" ||
					currentOperationVerb == "sizeNE" ||
					currentOperationVerb == "sizeLT" ||
					currentOperationVerb == "sizeLE" ||
					currentOperationVerb == "sizeGT" ||
					currentOperationVerb == "sizeGE") {
    				if(lhsAttribType != "rstring" &&
    					lhsAttribType != "set<int32>" && lhsAttribType != "set<int64>" &&
    					lhsAttribType != "set<float32>" && lhsAttribType != "set<float64>" &&
						lhsAttribType != "set<rstring>" && lhsAttribType != "list<rstring>" &&
						lhsAttribType != "list<int32>" && lhsAttribType != "list<int64>" &&
						lhsAttribType != "list<float32>" && lhsAttribType != "list<float64>" &&
						Functions::String::findFirst(lhsAttribType, "list<tuple<") != 0 &&
						lhsAttribType != "map<rstring,rstring>" &&
						lhsAttribType != "map<rstring,int32>" && lhsAttribType != "map<int32,rstring>" &&
						lhsAttribType != "map<rstring,int64>" && lhsAttribType != "map<int64,rstring>" &&
						lhsAttribType != "map<rstring,float32>" && lhsAttribType != "map<float32,rstring>" &&
						lhsAttribType != "map<rstring,float64>" && lhsAttribType != "map<float64,rstring>" &&
						lhsAttribType != "map<int32,int32>" && lhsAttribType != "map<int32,int64>" &&
						lhsAttribType != "map<int64,int32>" && lhsAttribType != "map<int64,int64>" &&
						lhsAttribType != "map<int32,float32>" && lhsAttribType != "map<int32,float64>" &&
						lhsAttribType != "map<int64,float32>" && lhsAttribType != "map<int64,float64>" &&
						lhsAttribType != "map<float32,int32>" && lhsAttribType != "map<float32,int64>" &&
						lhsAttribType != "map<float64,int32>" && lhsAttribType != "map<float64,int64>" &&
						lhsAttribType != "map<float32,float32>" && lhsAttribType != "map<float32,float64>" &&
						lhsAttribType != "map<float64,float32>" && lhsAttribType != "map<float64,float64>") {
    					// This operation verb is not allowed for a given LHS attribute type.
    					if(currentOperationVerb == "contains") {
    						error = INCOMPATIBLE_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notContains") {
    						error = INCOMPATIBLE_NOT_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "containsCI") {
    						error = INCOMPATIBLE_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notContainsCI") {
    						error = INCOMPATIBLE_NOT_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "sizeEQ") {
    						error = INCOMPATIBLE_SIZE_EQ_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "sizeNE") {
    						error = INCOMPATIBLE_SIZE_NE_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "sizeLT") {
    						error = INCOMPATIBLE_SIZE_LT_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "sizeLE") {
    						error = INCOMPATIBLE_SIZE_LE_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "sizeGT") {
    						error = INCOMPATIBLE_SIZE_GT_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else {
    						error = INCOMPATIBLE_SIZE_GE_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					}

    					return(false);
    				} else {
    					// We must do one more validation with lists and maps for their
    					// existence and size checks when they have non-string item types without
    					// index based access. If it doesn't pass this check, we should not
    					// allow the existence and size check operations.
    					if(lhsSubscriptForListAndMapAdded == true &&
    						(lhsAttribType == "list<int32>" || lhsAttribType == "list<int64>" ||
    						lhsAttribType == "list<float32>" || lhsAttribType == "list<float64>" ||
							lhsAttribType == "map<rstring,int32>" || lhsAttribType == "map<rstring,int64>" ||
							lhsAttribType == "map<rstring,float32>" || lhsAttribType == "map<rstring,float64>" ||
							lhsAttribType == "map<int32,int32>" || lhsAttribType == "map<int32,int64>" ||
							lhsAttribType == "map<int64,int32>" || lhsAttribType == "map<int64,int64>" ||
							lhsAttribType == "map<int32,float32>" || lhsAttribType == "map<int32,float64>" ||
							lhsAttribType == "map<int64,float32>" || lhsAttribType == "map<int64,float64>" ||
							lhsAttribType == "map<float32,int32>" || lhsAttribType == "map<float32,int64>" ||
							lhsAttribType == "map<float64,int32>" || lhsAttribType == "map<float64,int64>" ||
							lhsAttribType == "map<float32,float32>" || lhsAttribType == "map<float32,float64>" ||
							lhsAttribType == "map<float64,float32>" || lhsAttribType == "map<float64,float64>")) {
    						// We can't allow this operation when list or map index access is
    						// in place for non-string based data is stored inside of them.
        					if(currentOperationVerb == "contains") {
        						error = INCOMPATIBLE_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "notContains") {
        						error = INCOMPATIBLE_NOT_CONTAINS_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "containsCI") {
        						error = INCOMPATIBLE_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "notContainsCI") {
        						error = INCOMPATIBLE_NOT_CONTAINS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "sizeEQ") {
        						error = INCOMPATIBLE_SIZE_EQ_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "sizeNE") {
        						error = INCOMPATIBLE_SIZE_NE_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "sizeLT") {
        						error = INCOMPATIBLE_SIZE_LT_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "sizeLE") {
        						error = INCOMPATIBLE_SIZE_LE_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else if(currentOperationVerb == "sizeGT") {
        						error = INCOMPATIBLE_SIZE_GT_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					} else {
        						error = INCOMPATIBLE_SIZE_GE_OPERATION_FOR_LHS_ATTRIB_TYPE;
        					}

        					return(false);
    					}

    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);

    	    			// Special operation verbs such as contains, notContains etc.
    	    			// must be followed by a space character.
    	    			if(idx < stringLength && myBlob[idx] != ' ') {
    	    				error = SPACE_NOT_FOUND_AFTER_SPECIAL_OPERATION_VERB;
    	    				return(false);
    	    			}
    				}
    			} // End of validating contains operator verbs.

    			// We will allow substring related operation verbs only for
    			// string based attributes in a non-set data type.
    			if(currentOperationVerb == "startsWith" ||
    				currentOperationVerb == "endsWith" ||
					currentOperationVerb == "notStartsWith" ||
					currentOperationVerb == "notEndsWith" ||
					currentOperationVerb == "startsWithCI" ||
					currentOperationVerb == "endsWithCI" ||
					currentOperationVerb == "equalsCI" ||
					currentOperationVerb == "notStartsWithCI" ||
					currentOperationVerb == "notEndsWithCI" ||
					currentOperationVerb == "notEqualsCI") {
    				if(lhsAttribType != "rstring" &&
						lhsAttribType != "list<rstring>" &&
						lhsAttribType != "map<rstring,rstring>" &&
						lhsAttribType != "map<int32,rstring>" &&
						lhsAttribType != "map<int64,rstring>" &&
						lhsAttribType != "map<float32,rstring>" &&
						lhsAttribType != "map<float64,rstring>") {
    					// This operator is not allowed for a given LHS attribute type.
    					if(currentOperationVerb == "startsWith") {
    						error = INCOMPATIBLE_STARTS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "endsWith") {
    						error = INCOMPATIBLE_ENDS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notStartsWith") {
    						error = INCOMPATIBLE_NOT_STARTS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notEndsWith") {
    						error = INCOMPATIBLE_NOT_ENDS_WITH_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "startsWithCI") {
    						error = INCOMPATIBLE_STARTS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "endsWithCI") {
    						error = INCOMPATIBLE_ENDS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "equalsCI") {
    						error = INCOMPATIBLE_EQUALS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notStartsWithCI") {
    						error = INCOMPATIBLE_NOT_STARTS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else if(currentOperationVerb == "notEndsWithCI") {
    						error = INCOMPATIBLE_NOT_ENDS_WITH_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					} else {
    						error = INCOMPATIBLE_NOT_EQUALS_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					}

    					return(false);
    				} else {
    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);

    	    			// Special operation verbs such as startsWith, endsWith etc.
    	    			// must be followed by a space character.
    	    			if(idx < stringLength && myBlob[idx] != ' ') {
    	    				error = SPACE_NOT_FOUND_AFTER_SPECIAL_OPERATION_VERB;
    	    				return(false);
    	    			}
    				}
    			} // End of validating string based starts, ends operator verbs.

    			// We will allow membership evaluation in a list string literal via
    			// in and inCI operation verbs. We will support this for
    			// any LHS tuple attribute that is of type int32, float64 and rstring.
    			// e-g: [1, 2, 3, 4]
    			// [1.4, 5.3, 98.65, 7.2]
    			// ["Developer", "Tester", "Admin", "Manager"]
    			if(currentOperationVerb == "in") {
    				// We will allow 'in' verb for int32, float64 and rstring LHS attributes.
    				if(lhsAttribType != "rstring" &&
						lhsAttribType != "list<rstring>" &&
						lhsAttribType != "map<rstring,rstring>" &&
						lhsAttribType != "map<int32,rstring>" &&
						lhsAttribType != "map<int64,rstring>" &&
						lhsAttribType != "map<float32,rstring>" &&
						lhsAttribType != "map<float64,rstring>" &&
						lhsAttribType != "int32" &&
						lhsAttribType != "float64") {
    					// This operator verb is not allowed for a given LHS attribute type.
    					error = INCOMPATIBLE_IN_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					return(false);
    				} else {
    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);

    	    			// Special operation verbs such as in must be
    	    			// followed by a space character.
    	    			if(idx < stringLength && myBlob[idx] != ' ') {
    	    				error = SPACE_NOT_FOUND_AFTER_SPECIAL_OPERATION_VERB;
    	    				return(false);
    	    			}
    				}
    			} // End of validating in operator verb.

    			if(currentOperationVerb == "inCI") {
    				// We will allow 'inCI' verb only for rstring.
    				// Because, case insensitivity makes sense only for rstring.
    				if(lhsAttribType != "rstring" &&
						lhsAttribType != "list<rstring>" &&
						lhsAttribType != "map<rstring,rstring>" &&
						lhsAttribType != "map<int32,rstring>" &&
						lhsAttribType != "map<int64,rstring>" &&
						lhsAttribType != "map<float32,rstring>" &&
						lhsAttribType != "map<float64,rstring>") {
    					// This operator verb is not allowed for a given LHS attribute type.
    					error = INCOMPATIBLE_IN_CI_OPERATION_FOR_LHS_ATTRIB_TYPE;
    					return(false);
    				} else {
    	    			// We have a compatible operation verb for a given LHS attribute type.
    	    			// Move the idx past the current operation verb.
    	    			idx += Functions::String::length(currentOperationVerb);

    	    			// Special operation verbs such as inCI must be
    	    			// followed by a space character.
    	    			if(idx < stringLength && myBlob[idx] != ' ') {
    	    				error = SPACE_NOT_FOUND_AFTER_SPECIAL_OPERATION_VERB;
    	    				return(false);
    	    			}
    				}
    			} // End of validating inCI operator verb.

    			// Store the current operation verb in the subexpression layout list.
				Functions::Collections::appendM(subexpressionLayoutList,
					currentOperationVerb);
				operationVerbFound = true;

				if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 7a ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Validation start index=" << validationStartIdx << endl;
					cout <<  "Subexpression layout list after validating an operation verb." << endl;
	    			ConstListIterator it = subexpressionLayoutList.getBeginIterator();

	    			while (it != subexpressionLayoutList.getEndIterator()) {
	    				ConstValueHandle myVal = *it;
	    				rstring const & myString = myVal;
	    				cout << myString << endl;
	    				it++;
	    			} // End of list iteration while loop.

	    			cout <<  "Intra nested subexpression logical operators map after validating an operation verb." << endl;
					ConstMapIterator it2 = intraNestedSubexpressionLogicalOperatorsMap.getBeginIterator();

					while (it2 != intraNestedSubexpressionLogicalOperatorsMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal2 = *it2;
						std::pair<rstring,rstring> const & myRStringRString = myVal2;
						cout << "NestedSubexpressionId=" << myRStringRString.first <<
							", Logical operator=" <<  myRStringRString.second << endl;
						it2++;
					}

	    			cout <<  "Inter subexpression logical operators list after validating an operation verb." << endl;
	    			ConstListIterator it3 = interSubexpressionLogicalOperatorsList.getBeginIterator();

	    			while (it3 != interSubexpressionLogicalOperatorsList.getEndIterator()) {
	    				ConstValueHandle myVal3 = *it3;
	    				rstring const & myString3 = myVal3;
	    				cout << myString3 << endl;
	    				it3++;
	    			}

					cout << "==== END eval_predicate trace 7a ====" << endl;
				} // End of if(trace == true)

				// Continue the outer while loop.
				continue;
    		} // End of if(lhsFound == true && operationVerbFound == false)

    		// If we have found an LHS, an operation verb but no RHS, we must now
    		// look for it to complete a sub-expression.
    		// e-g:
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(lhsFound == true && operationVerbFound == true && rhsFound == false) {
    			// All we expect to see in an RHS is one of boolean, integer,
    			// float, string that is present there for comparison and
    			// completion of the sub-expression formed by the LHS and the operation verb.
    			//
    			// Let us see if the RHS carries the correct value for a given LHS attribute type.
    			//
    			// Attribute type we are interested is at index n-3 as of now.
    	    	// This list will have a sequence of rstring items as shown below.
    	    	// LHSAttribName
    	    	// LHSAttribType
    	    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	    	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
    	    	// RHSValue
    	    	// Intra subexpression logical operator - When N/A, it will have an empty string.
    	    	// ...   - The sequence above repeats for this subexpression.
    	    	//
    			// e-g:
    			// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    			// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    			rstring lhsAttribType = subexpressionLayoutList[
					Functions::Collections::size(subexpressionLayoutList) - 3];
    			rstring rhsValue = "";

    			if(lhsAttribType == "boolean") {
    				// It is straightforward. We can only have true or false as RHS.
    				// An RHS should be followed by either a space or a ) or
    				// it is the final token at the very end of the full expression.
    				if(Functions::String::findFirst(expr, "true ", idx) == idx ||
    					Functions::String::findFirst(expr, "true)", idx) == idx ||
						(Functions::String::findFirst(expr, "true", idx) == idx &&
						idx+4 == stringLength)) {
    					rhsValue = "true";
    					// Move the idx past the RHS value.
    					idx += 4;
    				} else if(Functions::String::findFirst(expr, "false ", idx) == idx ||
    					Functions::String::findFirst(expr, "false)", idx) == idx ||
						(Functions::String::findFirst(expr, "false", idx) == idx &&
						idx+5 == stringLength)) {
    					rhsValue = "false";
    					// Move the idx past the RHS value.
    					idx += 5;
    				}

    				if(rhsValue == "") {
    					error = RHS_VALUE_NO_MATCH_FOR_BOOLEAN_LHS_TYPE;
    					return(false);
    				}
    			} // End of if(lhsAttribType == "boolean")

    			// RHS integer value validation has to handle the collection item
    			// existence check operation in a specific way because we support
    			// map key existence check which means key is the first part in a map.
    			// So, we do a separate check for that and then the rest of the
    			// LHS integer usage. Note that when it comes to a map,
				// we support contains and notContains only for the map key.
				if(((currentOperationVerb != "contains" &&
	    			currentOperationVerb != "notContains" &&
					currentOperationVerb != "containsCI" &&
					currentOperationVerb != "notContainsCI" &&
					currentOperationVerb != "in" &&
	    			Functions::String::findFirst(currentOperationVerb, "size") != 0) &&
					(lhsAttribType == "int32" ||
					lhsAttribType == "uint32" || lhsAttribType == "int64" ||
					lhsAttribType == "uint64" ||
					lhsAttribType == "set<int32>" || lhsAttribType == "set<int64>" ||
					lhsAttribType == "list<int32>" || lhsAttribType == "list<int64>" ||
					lhsAttribType == "map<rstring,int32>" ||
					lhsAttribType == "map<rstring,int64>" ||
					lhsAttribType == "map<int32,int32>" || lhsAttribType == "map<int32,int64>" ||
					lhsAttribType == "map<int64,int32>" || lhsAttribType == "map<int64,int64>" ||
					lhsAttribType == "map<float32,int32>" || lhsAttribType == "map<float32,int64>" ||
					lhsAttribType == "map<float64,int32>" || lhsAttribType == "map<float64,int64>")) ||
					((currentOperationVerb == "contains" ||
					currentOperationVerb == "notContains" ||
					currentOperationVerb == "containsCI" ||
					currentOperationVerb == "notContainsCI") &&
					(lhsAttribType == "set<int32>" || lhsAttribType == "set<int64>" ||
					lhsAttribType == "list<int32>" || lhsAttribType == "list<int64>" ||
					(lhsAttribType == "map<int32,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<int64,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<int32,float32>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<int64,float32>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<int32,float64>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<int64,float64>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<rstring,int32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<rstring,int64>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<float32,int32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<float32,int64>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<float64,int32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<float64,int64>" && lhsSubscriptForListAndMapAdded == true) ||
					lhsAttribType == "map<int32,int32>" || lhsAttribType == "map<int32,int64>" ||
					lhsAttribType == "map<int64,int32>" || lhsAttribType == "map<int64,int64>"))) {
					// If the LHS attribute type is a signed integer, then we should
					// prepare to accept - sign preceding the numeral value.
					boolean signedIntegerAllowed = false;

					if(lhsAttribType == "int32" ||
						lhsAttribType == "int64" ||
						lhsAttribType == "set<int32>" ||
						lhsAttribType == "set<int64>" ||
						lhsAttribType == "list<int32>" ||
						lhsAttribType == "list<int64>" ||
						lhsAttribType == "map<rstring,int32>" ||
						lhsAttribType == "map<rstring,int64>" ||
						lhsAttribType == "map<int32,int32>" || lhsAttribType == "map<int32,int64>" ||
						lhsAttribType == "map<int64,int32>" || lhsAttribType == "map<int64,int64>" ||
						lhsAttribType == "map<float32,int32>" || lhsAttribType == "map<float32,int64>" ||
						lhsAttribType == "map<float64,int32>" || lhsAttribType == "map<float64,int64>" ||
						(lhsAttribType == "map<int32,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
						(lhsAttribType == "map<int64,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
						(lhsAttribType == "map<int32,float32>" && lhsSubscriptForListAndMapAdded == false) ||
						(lhsAttribType == "map<int64,float32>" && lhsSubscriptForListAndMapAdded == false) ||
						(lhsAttribType == "map<int32,float64>" && lhsSubscriptForListAndMapAdded == false) ||
						(lhsAttribType == "map<int64,float64>" && lhsSubscriptForListAndMapAdded == false)) {
						signedIntegerAllowed = true;
					}

					// RHS can only have numeral digits.
					boolean allNumeralsFound = false;
					int32 negativeSignCnt = 0;
					boolean negtiveSignAfterTheNumeralValue = false;

					// In this while loop, we will try to parse the numeral value that
					// appears as the RHS value.
					while(idx < stringLength) {
						// We can process until we get a space or a )
						if(myBlob[idx] == ' ') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] == ')') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
							// A non-numeral character found.
							// If it is a - sign, we can allow only one of it
							// just before the numeral value.
							if(signedIntegerAllowed == true && myBlob[idx] == '-' ) {
								if(rhsValue != "") {
									negativeSignCnt++;
									// We can't allow negative sign in the middle of a number.
									negtiveSignAfterTheNumeralValue = true;
									break;
								}

								if(negativeSignCnt < 1) {
									// One negative sign is allowed for an int value.
									negativeSignCnt++;
									rhsValue += "-";
									idx++;
									continue;
								} else {
									// We are seeing more than one decimal point.
									// This is not allowed.
									negativeSignCnt++;
									break;
								}
							}

							allNumeralsFound = false;
							break;
						} else {
							allNumeralsFound = true;
							rhsValue += myBlob[idx];
						}

						idx++;
					} // End of the inner while loop.

					if(negativeSignCnt > 1) {
						error = MORE_THAN_ONE_NEGATIVE_SIGN_IN_AN_RHS_INTEGER;
						return(false);
					}

					if(negtiveSignAfterTheNumeralValue == true) {
						error = NEGATIVE_SIGN_AT_WRONG_POSITION_OF_AN_RHS_INTEGER;
						return(false);
					}

					if(allNumeralsFound == false) {
						error = RHS_VALUE_NO_MATCH_FOR_INTEGER_LHS_TYPE;
						return(false);
					}
				} // End of if(lhsAttribType == int32 ...

    			// RHS float value validation has to handle the collection item
    			// existence check operation in a specific way because we support
    			// map key existence check which means key is the first part in a map.
    			// So, we do a separate check for that and then the rest of the
    			// LHS float usage. Note that when it comes to a map,
				// we support contains and notContains only for the map key.
				if(((currentOperationVerb != "contains" &&
		    		currentOperationVerb != "notContains" &&
					currentOperationVerb != "containsCI" &&
					currentOperationVerb != "notContainsCI" &&
					currentOperationVerb != "in" &&
					Functions::String::findFirst(currentOperationVerb, "size") != 0) &&
					(lhsAttribType == "float32" ||
					lhsAttribType == "float64" ||
					lhsAttribType == "set<float32>" || lhsAttribType == "set<float64>" ||
					lhsAttribType == "list<float32>" || lhsAttribType == "list<float64>" ||
					lhsAttribType == "map<rstring,float32>" ||
					lhsAttribType == "map<rstring,float64>" ||
					lhsAttribType == "map<int32,float32>" || lhsAttribType == "map<int32,float64>" ||
					lhsAttribType == "map<int64,float32>" || lhsAttribType == "map<int64,float64>" ||
					lhsAttribType == "map<float32,float32>" || lhsAttribType == "map<float32,float64>" ||
					lhsAttribType == "map<float64,float32>" || lhsAttribType == "map<float64,float64>")) ||
					((currentOperationVerb == "contains" ||
					currentOperationVerb == "notContains" ||
					currentOperationVerb == "containsCI" ||
					currentOperationVerb == "notContainsCI") &&
					(lhsAttribType == "set<float32>" || lhsAttribType == "set<float64>" ||
					lhsAttribType == "list<float32>" || lhsAttribType == "list<float64>" ||
					(lhsAttribType == "map<float32,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<float64,rstring>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<float32,int32>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<float64,int32>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<float32,int64>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<float64,int64>" && lhsSubscriptForListAndMapAdded == false) ||
					(lhsAttribType == "map<rstring,float32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<rstring,float64>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<int32,float32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<int32,float64>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<int64,float32>" && lhsSubscriptForListAndMapAdded == true) ||
					(lhsAttribType == "map<int64,float64>" && lhsSubscriptForListAndMapAdded == true) ||
					lhsAttribType == "map<float32,float32>" ||
					lhsAttribType == "map<float64,float32>" ||
					lhsAttribType == "map<float32,float64>" ||
					lhsAttribType == "map<float64,float64>"))) {
					// RHS can only have numeral digits with only one decimal point.
					boolean allNumeralsFound = false;
					int32 decimalPointCnt = 0;
					int32 negativeSignCnt = 0;
					boolean negtiveSignAfterTheNumeralValue = false;

					// In this while loop, we will try to parse the numeral value that
					// appears as the RHS value.
					while(idx < stringLength) {
						// We can process until we get a space or a )
						if(myBlob[idx] == ' ') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] == ')') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
							// If it is a - sign, we can allow only one of it
							// just before the numeral value.
							if(myBlob[idx] == '-' ) {
								if(rhsValue != "") {
									negativeSignCnt++;
									// We can't allow negative sign in the middle of a number.
									negtiveSignAfterTheNumeralValue = true;
									break;
								}

								if(negativeSignCnt < 1) {
									// One negative sign is allowed for an int value.
									negativeSignCnt++;
									rhsValue += "-";
									idx++;
									continue;
								} else {
									// We are seeing more than one negative sign.
									// This is not allowed.
									negativeSignCnt++;
									break;
								}
							}

							// If it is not a numeral, we can only allow
							// only one decimal point either in the beginning or
							// in the middle or in the end of the given numeral value.
							if(myBlob[idx] == '.' ) {
								if(decimalPointCnt < 1) {
									// One decimal point is allowed for a float value.
									decimalPointCnt++;
									rhsValue += ".";
									idx++;
									continue;
								} else {
									// We are seeing more than one decimal point.
									// This is not allowed.
									decimalPointCnt++;
									break;
								}
							}

							// A non-numeral character found.
							allNumeralsFound = false;
							break;
						} else {
							allNumeralsFound = true;
							rhsValue += myBlob[idx];
						}

						idx++;
					} // End of the inner while loop.

					if(negativeSignCnt > 1) {
						error = MORE_THAN_ONE_NEGATIVE_SIGN_IN_AN_RHS_FLOAT;
						return(false);
					}

					if(negtiveSignAfterTheNumeralValue == true) {
						error = NEGATIVE_SIGN_AT_WRONG_POSITION_OF_AN_RHS_FLOAT;
						return(false);
					}

					if(decimalPointCnt == 0) {
						// No decimal point in the numeral value.
						error = NO_DECIMAL_POINT_IN_RHS_VALUE;
						return(false);
					}

					if(decimalPointCnt > 1) {
						// More than one decimal point in the numeral value.
						error = MORE_THAN_ONE_DECIMAL_POINT_IN_RHS_VALUE;
						return(false);
					}

					if(allNumeralsFound == false) {
						error = RHS_VALUE_NO_MATCH_FOR_FLOAT_LHS_TYPE;
						return(false);
					}
				} // End of if(lhsAttribType == "float32" ...

    			// RHS string value validation has to handle the collection item
    			// existence check operation in a specific way because we support
    			// map key existence check which means key is the first part in a map.
    			// So, we do a separate check for that and then the rest of the
    			// LHS string usage. Note that when it comes to a map,
				// we support contains and notContains only for the map key.
				if(((currentOperationVerb != "contains" &&
		    		currentOperationVerb != "notContains" &&
					currentOperationVerb != "containsCI" &&
					currentOperationVerb != "notContainsCI" &&
					currentOperationVerb != "in" &&
					currentOperationVerb != "inCI" &&
					Functions::String::findFirst(currentOperationVerb, "size") != 0) &&
					(lhsAttribType == "rstring" ||
					lhsAttribType == "set<rstring>" ||
					lhsAttribType == "list<rstring>" ||
					lhsAttribType == "map<int32,rstring>" ||
					lhsAttribType == "map<int64,rstring>" ||
					lhsAttribType == "map<float32,rstring>" ||
					lhsAttribType == "map<float64,rstring>" ||
					lhsAttribType == "map<rstring,rstring>")) ||
					((currentOperationVerb == "contains" ||
					currentOperationVerb == "notContains" ||
					currentOperationVerb == "containsCI" ||
					currentOperationVerb == "notContainsCI") &&
					(lhsAttribType == "rstring" ||
					 lhsAttribType == "set<rstring>" || lhsAttribType == "list<rstring>" ||
					 (lhsAttribType == "map<rstring,int32>" && lhsSubscriptForListAndMapAdded == false) ||
					 (lhsAttribType == "map<rstring,int64>" && lhsSubscriptForListAndMapAdded == false) ||
					 (lhsAttribType == "map<rstring,float32>" && lhsSubscriptForListAndMapAdded == false) ||
					 (lhsAttribType == "map<rstring,float64>" && lhsSubscriptForListAndMapAdded == false) ||
					 (lhsAttribType == "map<int32,rstring>" && lhsSubscriptForListAndMapAdded == true) ||
					 (lhsAttribType == "map<int64,rstring>" && lhsSubscriptForListAndMapAdded == true) ||
					 (lhsAttribType == "map<float32,rstring>" && lhsSubscriptForListAndMapAdded == true) ||
					 (lhsAttribType == "map<float64,rstring>" && lhsSubscriptForListAndMapAdded == true) ||
					 lhsAttribType == "map<rstring,rstring>"))) {
					// RHS can have any character from space to ~ i.e. character 20 t0 126.
					// Very first character must be ' or ".
					boolean openQuoteFound = false;
					boolean closeQuoteFound = false;

					if(myBlob[idx] == '\'' || myBlob[idx] == '"') {
						openQuoteFound = true;
						// Move past the open quote character.
						idx++;
					}

					if(openQuoteFound == false) {
						error = RHS_VALUE_WITH_MISSING_OPEN_QUOTE_NO_MATCH_FOR_STRING_LHS_TYPE;
						return(false);
					}

					// Stay in a loop and collect all the values until
					// we see a close quote.
					while(idx < stringLength) {
						// Do we have an RHS string end indicator?
						// (single or double quote)?
						if(isQuoteCharacterAtEndOfRhsString(myBlob, idx) == true) {
							closeQuoteFound = true;
							// Move past the close quote character.
							idx++;
							break;
						} else {
							// We will keep collecting any other character including
							// the single or double quote character that is
							// embedded as part of the RHS string.
							rhsValue += myBlob[idx];
						}

						idx++;
					} // End of the inner while loop.

					if(closeQuoteFound == false) {
						error = RHS_VALUE_WITH_MISSING_CLOSE_QUOTE_NO_MATCH_FOR_STRING_LHS_TYPE;
						return(false);
					}
				} // End of if(lhsAttribType == "rstring" ...

				// If the operation verb is in or inCI, then the RHS
				// must follow a list string literal format.
				// e-g: [1, 2, 3, 4]
				// [1.4, 5.3, 98.65, 7.2]
				// ["Developer", "Tester", "Admin", "Manager"]
				if(currentOperationVerb == "in" ||
					currentOperationVerb == "inCI") {
					// The necessary check to ensure that the in or inCI
					// operation verb is only associated with a int32, float64 and
					// rstring based LHS was already done in the LHS validation stage above.
					//
					// In this case, RHS must start with [ and end with ].
					// RHS can have any character from space to ~ i.e. character 20 t0 126.
					// Very first character must be [.
					boolean openBracketFound = false;
					boolean closeBracketFound = false;

					if(myBlob[idx] == '[') {
						openBracketFound = true;
						// Let us store it as part of the RHS value.
						rhsValue += myBlob[idx];
						// Move past the open bracket character.
						idx++;
					}

					if(openBracketFound == false) {
						error = RHS_VALUE_WITH_MISSING_OPEN_BRACKET_NO_MATCH_FOR_IN_OR_IN_CI_OPVERB;
						return(false);
					}

					// Stay in a loop and collect all the values until
					// we see a close bracket.
					while(idx < stringLength) {
						// Do we have an RHS list literal end indicator i.e. a clost bracket?
						if(isCloseBracketAtEndOfRhsString(myBlob, idx) == true) {
							closeBracketFound = true;
							// Let us store it as part of the RHS value.
							rhsValue += myBlob[idx];
							// Move past the close bracket character.
							idx++;
							break;
						} else {
							// We will keep collecting any other character including
							// the open or close bracket that is
							// embedded as part of the RHS list literal.
							rhsValue += myBlob[idx];
						}

						idx++;
					} // End of the inner while loop.

					if(closeBracketFound == false) {
						error = RHS_VALUE_WITH_MISSING_CLOSE_BRACKET_NO_MATCH_FOR_IN_OR_IN_CI_OPVERB;
						return(false);
					}
				} // End of processing RHS for in or inCI.

				// If the operation verb is sizeXX, then the RHS
				// must be an integer value without a sign.
				if(currentOperationVerb == "sizeEQ" ||
					currentOperationVerb == "sizeNE" ||
					currentOperationVerb == "sizeLT" ||
					currentOperationVerb == "sizeLE" ||
					currentOperationVerb == "sizeGT" ||
					currentOperationVerb == "sizeGE") {
					// The necessary check to ensure that the sizeXX
					// operation verb is only associated with a
					// non indexed plain collection type i.e. set, list or map was
					// already done in the LHS and OpVerb validation stages above.
					//
					// In this case, RHS can only have numeral digits.
					boolean allNumeralsFound = false;

					// In this while loop, we will try to parse the numeral value that
					// appears as the RHS value.
					while(idx < stringLength) {
						// We can process until we get a space or a )
						if(myBlob[idx] == ' ') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] == ')') {
							// The numeral part ended at the previous position.
							break;
						} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
							// A non-numeral character found.
							allNumeralsFound = false;
							break;
						} else {
							allNumeralsFound = true;
							rhsValue += myBlob[idx];
						}

						idx++;
					} // End of the inner while loop.

					if(allNumeralsFound == false) {
						error = RHS_VALUE_NO_MATCH_FOR_SIZEXX_OPERATION_VERB;
						return(false);
					}
				} // End of if(currentOperationVerb == "sizeEQ" ||

				// If RHS value was not properly derived in the many
				// different if conditional blocks above, we will return an error.
				if(rhsValue == "") {
					error = UNABLE_TO_PARSE_RHS_VALUE;
					return(false);
				}

    			// Store the current RHS value in the subexpression layout list.
				Functions::Collections::appendM(subexpressionLayoutList, rhsValue);
				rhsFound = true;

				if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 8a ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Validation start index=" << validationStartIdx << endl;
					cout <<  "Subexpression layout list after validating an RHS." << endl;
	    			ConstListIterator it = subexpressionLayoutList.getBeginIterator();

	    			while (it != subexpressionLayoutList.getEndIterator()) {
	    				ConstValueHandle myVal = *it;
	    				rstring const & myString = myVal;
	    				cout << myString << endl;
	    				it++;
	    			} // End of list iteration while loop.

	    			cout <<  "Intra nested subexpression logical operators map after validating an RHS." << endl;
					ConstMapIterator it2 = intraNestedSubexpressionLogicalOperatorsMap.getBeginIterator();

					while (it2 != intraNestedSubexpressionLogicalOperatorsMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal2 = *it2;
						std::pair<rstring,rstring> const & myRStringRString = myVal2;
						cout << "NestedSubexpressionId=" << myRStringRString.first <<
							", Logical operator=" <<  myRStringRString.second << endl;
						it2++;
					}

	    			cout <<  "Inter subexpression logical operators list after validating an RHS." << endl;
	    			ConstListIterator it3 = interSubexpressionLogicalOperatorsList.getBeginIterator();

	    			while (it3 != interSubexpressionLogicalOperatorsList.getEndIterator()) {
	    				ConstValueHandle myVal3 = *it3;
	    				rstring const & myString3 = myVal3;
	    				cout << myString3 << endl;
	    				it3++;
	    			}

					cout << "==== END eval_predicate trace 8a ====" << endl;
				} // End of if(trace == true)

				// If we are on a recursive call to this method, it usually
				// happens to validate only a subexpression that
				// involves list<TUPLE>. When the LHS, OperationVerb and RHS are
				// done for that recursive call, we can return instead of
				// proceeding further.
				if(validationStartIdx > 0) {
					// It is necessary to set the validationStartIdx where
					// we left off before returning from here. That will
					// give the recursive caller a new position in the
					// expression from where to continue further validation.
					validationStartIdx = idx;
					return(true);
				} else {
					// Continue the outer while loop.
					continue;
				}
    		} // End of if(lhsFound == true && operationVerbFound == true && rhsFound == false)

    		// If we have found an LHS, an operation verb and an RHS, we can now
    		// look for a logical operator to follow it.
    		// e-g: a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		// e-g: (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		if(lhsFound == true && operationVerbFound == true && rhsFound == true) {
    			// We have just now completed processing either part of a
    			// subexpression or the full sub-expression.
    			// So, the next one we can have is one of the allowed logical operators.
    			// We have to check whether we are still within a subexpression or we
    			// just now got done with a full subexpression. This check is needed to ensure that
    			// within a given subexpression, only a same kind of logical operator gets used.
    			rstring logicalOperatorUsedWithinSubexpression = "";

    			if(openParenthesisCnt != closeParenthesisCnt) {
    				// We are still within a subexpression.
    				// Save the logical operator that has been in use within the
    				// current subexpression we are processing.
    				logicalOperatorUsedWithinSubexpression = mostRecentLogicalOperatorFound;
    			}

    			mostRecentLogicalOperatorFound = "";
    			ConstListIterator it = logicalOperationsList.getBeginIterator();

    			while (it != logicalOperationsList.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				rstring const & myLogicalOp = myVal;

    				if(Functions::String::findFirst(expr, myLogicalOp, idx) == idx) {
    					// We have a logical operator match.
    					mostRecentLogicalOperatorFound = myLogicalOp;
    					break;
    				}

    				// Continue to iterate.
    				it++;
    			} // End of list iterator while loop.

    			if(mostRecentLogicalOperatorFound == "") {
    				// We don't have a match with a valid operator.
    				error = INVALID_LOGICAL_OPERATOR_FOUND_IN_EXPRESSION;
    				return(false);
    			}

    			// Do the check necessary to ensure that we are encountering the
    			// same logical operator within a subexpression.
    			if(openParenthesisCnt != closeParenthesisCnt &&
    				multiPartSubexpressionPartsCnt > 0 &&
    				mostRecentLogicalOperatorFound != logicalOperatorUsedWithinSubexpression) {
    				// We can't allow mixing of logical operators within a subexpression.
    				error = MIXED_LOGICAL_OPERATORS_FOUND_IN_SUBEXPRESSION;
    				return(false);
    			}

    			// A logical operator must have a space character right before it.
    			if(idx > 0 && myBlob[idx-1] != ' ') {
    				error = NO_SPACE_RIGHT_BEFORE_LOGICAL_OPERATOR;
    				return(false);
    			}

    			// Now that we have found a valid logical operator, we can
    			// reset the sub-expression component status flags in order to
    			// get ready to process an upcoming new sub-expression.
    			lhsFound = false;
    			operationVerbFound = false;
    			rhsFound = false;
    			lhsPrecededByOpenParenthesis = false;
    			// Set this flag to indicate where we are now.
    			logicalOperatorFound = true;
				int32 selolSize =
					Functions::Collections::size(subexpressionLayoutList);

        		// e-g: a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
        		// e-g: (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    			//
    			// If we are still within the same subexpression, then we will append
    			// an entry for the current logical operator in the subexpression layout list.
    	    	// This list will have a sequence of rstring items as shown below.
    	    	// LHSAttribName
    	    	// LHSAttribType
    	    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	    	// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
    	    	// RHSValue
    	    	// Intra subexpression logical operator - When N/A, it will have an empty string.
    	    	// ...   - The sequence above repeats for this subexpression.
    	    	//
    			if(openParenthesisCnt != closeParenthesisCnt) {
    				// Do a special step for assisting with the
    				// processing of nested subexpressions.
    				// There are two either this or that conditions in this if block.
    				// In my tests, I found out that the first condition itself
    				// is sufficient. The second condition to check for the
    				// presence of consecutive CP is redundant. At a later time,
    				// we can test with more nested expressions to confirm this
    				// and then remove the second condition altogether.
    				// For now, having it here causes no harm.
    				if((currentNestedSubexpressionLevel > 0 && selolSize == 0) ||
    					(consecutiveCloseParenthesisFound == true)){
						// Insert the logical operator value into a separate map where
    					// the intra nested subexpression logical operators are kept.
						Functions::Collections::insertM(intraNestedSubexpressionLogicalOperatorsMap,
							subexpressionId, mostRecentLogicalOperatorFound);
						multiPartSubexpressionPartsCnt = 0;
						consecutiveCloseParenthesisFound = false;
    				} else {
						// We are still within the same subexpression.
						// Append the current intra subexpression logical operator we just found.
    					// This else block will cover both the cases where we are
    					// either within a nested or non-nested subexpression.
    					// In the nested case, we will come to this else block only
    					// when SELOL has non-zero size. Refer to the nested
    					// subexpression processing steps 3b1 and 3b2 at the
    					// top of this method.
						Functions::Collections::appendM(subexpressionLayoutList,
							mostRecentLogicalOperatorFound);
						// Increment the parts count for this multi-part subexpression.
						multiPartSubexpressionPartsCnt++;
    				}
    			} else {
					// We can now add this inter subexpression logical separator to
    				// the list where we will keep them for later use.
    				Functions::Collections::appendM(interSubexpressionLogicalOperatorsList,
    					mostRecentLogicalOperatorFound);

    				// We are now just outside of a subexpression that was processed
    				// completely. The next one will be a new subexpression.
    				// So let us append an empty string in the logical operator
    				// slot of the subexpression layout list to indicate the end of
    				// that completed subexpression.
    				// We can do this only if we come to this else block after
    				// processing a non-nested subexpression in which case we will
    				// have the SELOL with non-zero size. If we come here after
    				// processing a nested subexpression, then SELOL will be
    				// empty in which case, we can skip inserting that empty
    				// SELOL into the SEMAP.
    				if(selolSize > 0) {
						rstring str = "";
						Functions::Collections::appendM(subexpressionLayoutList, str);
	    				// We can now add this completed subexpression layout list to
	    				// the subexpressions map.
	    		    	// Let us get a map key now (e-g: 1.1, 4.1 etc.)
	    				// First argument 0 indicates that it is not a
	    				// nested subexpression that just got processed.
	    				getNextSubexpressionId(0, subexpressionId);
	    		    	Functions::Collections::insertM(subexpressionsMap,
	    		    		subexpressionId, subexpressionLayoutList);
	    		    	// We can clear the subexpression layout list now to
	    		    	// start processing the next new subexpression.
	    		    	Functions::Collections::clearM(subexpressionLayoutList);
    				}

    		    	// We may encounter a brand new subexpression. So, reset this counter.
    		    	multiPartSubexpressionPartsCnt = 0;
    		    	// Reset this value just as a safety measure.
    		    	currentNestedSubexpressionLevel = 0;
    			}

    			// We have to ensure that the interSubexpressionLogicalOperatorsList
    			// contains a homogeneous set of logical operators.
    			// We need the following logic to check for that condition.
				SPL::set<rstring> mySet =
					Functions::Collections::toSet(interSubexpressionLogicalOperatorsList);

				if(Functions::Collections::size(mySet) > 1) {
					// In the zero parenthesis expression, we can have
					// only one kind of logical operator.
					error = MIXED_LOGICAL_OPERATORS_FOUND_IN_INTER_SUBEXPRESSIONS;
					return(false);
				}

    			// Move the idx past the logical operator characters.
    			idx += Functions::String::length(mostRecentLogicalOperatorFound);

    			// A logical operator must have a space character right after it.
    			if((idx < stringLength) && (myBlob[idx] != ' ')) {
    				error = NO_SPACE_RIGHT_AFTER_LOGICAL_OPERATOR;
    				return(false);
    			}

				if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 9a ====" << endl;
					cout << "Full expression=" << expr << endl;
					cout << "Validation start index=" << validationStartIdx << endl;
					cout << "currentNestedSubexpressionLevel=" <<
						currentNestedSubexpressionLevel << endl;
					cout << "multiPartSubexpressionPartsCnt=" <<
						multiPartSubexpressionPartsCnt << endl;

					cout <<  "Most recent logical operator found is " <<
						mostRecentLogicalOperatorFound << endl;

					cout <<  "Subexpression layout list after validating a logical operator." << endl;
	    			ConstListIterator it = subexpressionLayoutList.getBeginIterator();

	    			while (it != subexpressionLayoutList.getEndIterator()) {
	    				ConstValueHandle myVal = *it;
	    				rstring const & myString = myVal;
	    				cout << myString << endl;
	    				it++;
	    			} // End of list iteration while loop.

	    			cout <<  "Intra nested subexpression logical operators map after validating a logical operator." << endl;
					ConstMapIterator it2 = intraNestedSubexpressionLogicalOperatorsMap.getBeginIterator();

					while (it2 != intraNestedSubexpressionLogicalOperatorsMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal2 = *it2;
						std::pair<rstring,rstring> const & myRStringRString = myVal2;
						cout << "NestedSubexpressionId=" << myRStringRString.first <<
							", Logical operator=" <<  myRStringRString.second << endl;
						it2++;
					}

	    			cout <<  "Inter subexpression logical operators list after validating a logical operator." << endl;
	    			ConstListIterator it3 = interSubexpressionLogicalOperatorsList.getBeginIterator();

	    			while (it3 != interSubexpressionLogicalOperatorsList.getEndIterator()) {
	    				ConstValueHandle myVal3 = *it3;
	    				rstring const & myString3 = myVal3;
	    				cout << myString3 << endl;
	    				it3++;
	    			}

	    			cout <<  "Subexpressions map after validating a logical operator." << endl;
					ConstMapIterator it4 = subexpressionsMap.getBeginIterator();
					int32 cnt  = 0;

					while (it4 != subexpressionsMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal4 = *it4;
						std::pair<rstring,SPL::list<rstring> > const & myRStringSplListRString = myVal4;
						cout << "Map Key" << ++cnt << "=" <<
							myRStringSplListRString.first << endl;

						cout << "Map value:" << endl;
						// Let us display the contents of the map value which is a list.
		    			SPL::list<rstring> const & myListRString = myRStringSplListRString.second;
		    			ConstListIterator it5 = myListRString.getBeginIterator();

		    			while (it5 != myListRString.getEndIterator()) {
		    				ConstValueHandle myVal5 = *it5;
		    				rstring const & myRString = myVal5;
		    				cout << myRString << endl;
		    				it5++;
		    			}

						it4++;
					}

					cout << "==== END eval_predicate trace 9a ====" << endl;
				} // End of if(trace == true)

				// Continue the outer while loop.
				continue;
    		} // End of if(lhsFound == true && operationVerbFound == true && rhsFound == true)
    	} // End of the outermost validation while loop.

    	if(openParenthesisCnt != closeParenthesisCnt) {
    		// This expression string reached an end without all parentheses getting
    		// paired up and processed.
    		error = UNPROCESSED_PARENTHESIS_FOUND_IN_EXPRESSION;
    	} else if(Functions::Collections::size(subexpressionsMap) == 0 &&
    		Functions::Collections::size(subexpressionLayoutList) == 0) {
    		// This can happen when the entire expression simply contains
    		// space characters, matching open/close parenthesis and
    		// no other characters.
    		error = EXPRESSION_WITH_NO_LHS_AND_OPERATION_VERB_AND_RHS;
    	} else if(lhsFound == true && operationVerbFound == true && rhsFound == true) {
    		// There is no sub-expression processing still pending.
    		// This means there was no logical operator after the finally
    		// validated sub-expression and hence we have these three flags set to
    		// true. This represents a clean expression validation.
    		error = ALL_CLEAR;

    		// Is there a pending subexpression layout list that
    		// needs to be added to the subexpressions map?
    		// This is usually the case when the very last
    		// subexpression is a non-nested one.
    		if(Functions::Collections::size(subexpressionLayoutList) > 0) {
				// Since it is the very final subexpression processed,
				// we have to add it to the subexpression map.
				// Let us append an empty string in the logical operator
				// slot of the subexpression layout list for that
				// completed subexpression.
				rstring str = "";
				Functions::Collections::appendM(subexpressionLayoutList, str);

				// We can now add this completed subexpression layout list to
				// the subexpressions map. Just because this is the very final
				// subexpression processed, open and close parenthesis count
				// must be the same at this point.
				// Let us get a map key now (e-g: 1.1, 4.1 etc.)
				// First argument 0 indicates that it is not a
				// nested subexpression that just got processed.
				getNextSubexpressionId(0, subexpressionId);
				Functions::Collections::insertM(subexpressionsMap,
					subexpressionId, subexpressionLayoutList);
    		}

			if(trace == true) {
				cout << "==== BEGIN eval_predicate trace 10a ====" << endl;
				cout << "Full expression=" << expr << endl;
				cout << "Validation start index=" << validationStartIdx << endl;
				cout << "currentNestedSubexpressionLevel=" <<
					currentNestedSubexpressionLevel << endl;
				cout << "multiPartSubexpressionPartsCnt=" <<
					multiPartSubexpressionPartsCnt << endl;

				cout <<  "Subexpression layout list after validating the full expression." << endl;
    			ConstListIterator it = subexpressionLayoutList.getBeginIterator();

    			while (it != subexpressionLayoutList.getEndIterator()) {
    				ConstValueHandle myVal = *it;
    				rstring const & myString = myVal;
    				cout << myString << endl;
    				it++;
    			} // End of list iteration while loop.

    			cout <<  "Intra nested subexpression logical operators map after validating the full expression." << endl;
				ConstMapIterator it2 = intraNestedSubexpressionLogicalOperatorsMap.getBeginIterator();

				while (it2 != intraNestedSubexpressionLogicalOperatorsMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal2 = *it2;
					std::pair<rstring,rstring> const & myRStringRString = myVal2;
					cout << "NestedSubexpressionId=" << myRStringRString.first <<
						", Logical operator=" <<  myRStringRString.second << endl;
					it2++;
				}

    			cout <<  "Inter subexpression logical operators list after validating the full expression." << endl;
    			ConstListIterator it3 = interSubexpressionLogicalOperatorsList.getBeginIterator();

    			while (it3 != interSubexpressionLogicalOperatorsList.getEndIterator()) {
    				ConstValueHandle myVal3 = *it3;
    				rstring const & myString3 = myVal3;
    				cout << myString3 << endl;
    				it3++;
    			}

    			cout <<  "Subexpressions map after validating the full expression." << endl;

				ConstMapIterator it4 = subexpressionsMap.getBeginIterator();
				int32 cnt  = 0;

				while (it4 != subexpressionsMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal4 = *it4;
					std::pair<rstring,SPL::list<rstring> > const & myRStringSplListRString = myVal4;
					cout << "Map Key" << ++cnt << "=" <<
						myRStringSplListRString.first << endl;

					cout << "Map value:" << endl;
					// Let us display the contents of the map value which is a list.
	    			SPL::list<rstring> const & myListRString = myRStringSplListRString.second;
	    			ConstListIterator it5 = myListRString.getBeginIterator();

	    			while (it5 != myListRString.getEndIterator()) {
	    				ConstValueHandle myVal5 = *it5;
	    				rstring const & myRString = myVal5;
	    				cout << myRString << endl;
	    				it5++;
	    			}

					it4++;
				}

				cout << "==== END eval_predicate trace 10a ====" << endl;
			} // End of if(trace == true)
    	} else if(lhsFound == true && operationVerbFound == true && rhsFound == false) {
    		// There is pending RHS processing.
    		error = UNPROCESSED_RHS_FOUND_IN_EXPRESSION;
    	} else if(lhsFound == true && operationVerbFound == false && rhsFound == false) {
    		// There is pending operation verb processing.
    		error = UNPROCESSED_OPERATION_VERB_FOUND_IN_EXPRESSION;
    	} else if(logicalOperatorFound == true) {
    		// Expression has a logical operator at the
    		// very end without any further lhs, opverb and rhs.
    		// It is incomplete.
    		error = INCOMPLETE_EXPRESSION_ENDING_WITH_LOGICAL_OPERATOR;
    	}

    	if(error == ALL_CLEAR) {
    		// If we happen to have intra nested subexpression logical
    		// operators in the map, they should all be of same kind within
    		// a given nested subexpression group. Let us verify that now.
    		//
			// Let us sort the map keys so that we can process them
    		// according to the nested subexpression id.
			SPL::list<rstring> nestedSubexpressionIds =
				Functions::Collections::keys(intraNestedSubexpressionLogicalOperatorsMap);
			Functions::Collections::sortM(nestedSubexpressionIds);
			int32 nestedSubexpressionIdsListSize =
				Functions::Collections::size(nestedSubexpressionIds);

			// Let us now check if the logical operators are of the
			// same kind within a nested subexpression major group.
			// e-g: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4
			// For an expression layout like the one shown above,
			// intra nested subexpression logical operator list will
			// look like this.
			// 1.1, 1.2, 2.1, 2.2, 2.3
			// As you can see, it will not have the last item in every
			// nested subexpression group. That is because there will
			// only be N-1 logical operators in a given nested group.
			// We will check for the logical operator homogeneity now.
			int32  previousSubExpressionId = -1;
			rstring previousLogicalOperator = "";

			for(int32 i=0; i<nestedSubexpressionIdsListSize; i++) {
				rstring idString = nestedSubexpressionIds[i];
				rstring currentLogicalOperator =
					intraNestedSubexpressionLogicalOperatorsMap.at(idString);
				SPL::list<rstring> tokens =
					Functions::String::tokenize(idString, ".", false);
				// We have to compare only at level 1 of the subexpression id.
				// e-g: 2.4  Level 1 is 2 and Level is 4.
				int32 currentId = atoi(tokens[0].c_str());

				if(currentId != previousSubExpressionId) {
					previousSubExpressionId = currentId;
					previousLogicalOperator = currentLogicalOperator;
					// This is a new nested group. So far good.
				} else {
					// Let us compare that the logical operator is the
					// same within the current nested group.
					if(currentLogicalOperator != previousLogicalOperator) {
						// This is not good. There is a logical operator mismatch.
		        		error = MIXED_LOGICAL_OPERATORS_FOUND_IN_NESTED_SUBEXPRESSIONS;
		        		return(false);
					}
				}
			} // End of for loop.

        	// If we happen to have inter subexpression logical operators,
        	// that should always be one less than the unique number of
			// subexpression ids present in the subexpressions map.
			// We will match the uniqueness only in level 1 of the
			// subexpression id.  e-g: 2.4  Level 1 is 2 and Level is 4.
			// We have to do this, because, there may be nested subexpressions.
        	int32 subexpMapSize =
        		Functions::Collections::size(subexpressionsMap);
        	int32 logicalOpListSize =
        		Functions::Collections::size(interSubexpressionLogicalOperatorsList);
			SPL::list<rstring> subexpressionIds =
				Functions::Collections::keys(subexpressionsMap);
			Functions::Collections::sortM(subexpressionIds);
			previousSubExpressionId = -1;
			int32 uniqueIdCnt = 0;

			for(int32 i=0; i<subexpMapSize; i++) {
				rstring idString = subexpressionIds[i];
				SPL::list<rstring> tokens =
					Functions::String::tokenize(idString, ".", false);
				// We have to compare only at level 1 of the subexpression id.
				// e-g: 2.4  Level 1 is 2 and Level is 4.
				int32 currentId = atoi(tokens[0].c_str());

				if(currentId != previousSubExpressionId) {
					previousSubExpressionId = currentId;
					// This is a new nested group. So far good.
					uniqueIdCnt++;
				}
			} // End of for loop.

        	if(logicalOpListSize != (uniqueIdCnt-1)) {
        		error = INCORRECT_NUMBER_OF_INTER_SUBEXPRESSION_LOGICAL_OPERATORS;
        		return(false);
        	} else {
        		// Successful expression validation.
        		return(true);
        	}
    	} else {
    		// Not a successful expression validation.
    		return(false);
    	}
    } // End of validateExpression
    // ====================================================================

    // ====================================================================
    // This method receives the evaluation plan pointer as input and
    // then runs the full evaluation of the associated expression.
    inline boolean evaluateExpression(ExpressionEvaluationPlan *evalPlanPtr,
    	Tuple const & myTuple, int32 & error, boolean trace=false) {
    	// This method will get called recursively when a list<TUPLE> is
    	// encountered in a given expression. It is important to note that
    	// the recursive caller must always pass its own newly formed
    	// local variables as method arguments here so as not to overwrite
    	// the original reference pointers passed by the very first caller
    	// who made the initial non-recursive call.
    	error = ALL_CLEAR;
    	int32 subexpressionCntInCurrentNestedGroup = 0;
    	rstring intraNestedSubexpressionLogicalOperator = "";
    	SPL::list<boolean> nestedSubexpressionEvalResults;
    	SPL::list<boolean> interSubexpressionEvalResults;

    	// We can find everything we need to perform the evaluation inside the
    	// eval plan class passed to this function. It contains the following members.
    	//
		// rstring expression  --> Needed when evaluation is needed for list<TUPLE>.
		// rstring tupleSchema  --> Not needed.
		// SPL::map<rstring, SPL::list<rstring> > subexpressionsMap  --> Needed very much.
		// SPL::list<rstring> subexpressionsMapKeys  --> Needed very much.
    	// SPL::map<rstring, rstring> intraNestedSubexpressionLogicalOperatorsMap  --> Needed very much.
		// SPL::list<rstring> interSubexpressionLogicalOperatorsList  --> Needed very much.
    	//
    	int32 subexpMapSize =
    		Functions::Collections::size(evalPlanPtr->getSubexpressionsMapKeys());

    	if(subexpMapSize == 0) {
    		// It is a very rare error. However, we will check for it.
    		error = ZERO_SUBEXPRESSIONS_MAP_KEYS_FOUND_DURING_EVAL;
    		return(false);
    	}

    	// We have to loop through all the map keys in the specified order and then
    	// process each subexpression. Some of the subexpressions may be within
    	// a nested subexpression group. So, they have to be evaluated together to
    	// obtain a single result for a given nested subexpression group before
    	// using that single result with the other such nested or non-nested
    	// subexpression groups.
    	for(int32 i=0; i<subexpMapSize; i++) {
    		// Get the map key.
    		rstring mapKey = evalPlanPtr->getSubexpressionsMapKeys()[i];

    		// This map's key is a subexpression id.
    		// Subexpression id will go something like this:
    		// 1.1, 1.2, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.1, 4.2, 4.3, 5.1
    		// Subexpression id is made of level 1 and level 2.
			// We support either zero parenthesis or a single level or
			// multilevel (nested) parenthesis.
    		// Logical operators used within a subexpression must be of the same kind.
			//
			// A few examples of zero, single and nested parenthesis.
    		//
    		// 1.1             2.1                 3.1           4.1
    		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
    		//
    		// 1.1                               2.1
    		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
    		//
    		// 1.1                2.1                   3.1             4.1
    		// (a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)
    		//
    		// 1.1                          2.1                       2.2
    		// (a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)
    		//
    		// 1.1                 2.1                       2.2
    		// (a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))
    		//
    		// 1.1                                 2.1
    		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
    		//
    		// 1.1                     2.1                 2.2
    		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))
    		//
    		// 1.1                                        1.2                           2.1
    		// ((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)
    		//
    		// 1.1                                                                   1.2                          1.3
    		// (((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')
    		//
    		// This map's value is a list that describes the composition of a given subexpression.
    		// Structure of such a list will go something like this:
    		// This list will have a sequence of rstring items as shown below.
    		// LHSAttribName
    		// LHSAttribType
    		// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    		// OperationVerb - For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
    		// RHSValue
    		// Intra subexpression logical operator - When N/A, it will have an empty string.
    		// ...   - The sequence above repeats for this subexpression.
    		//
    		if(Functions::Collections::has(evalPlanPtr->getSubexpressionsMap(),
    			mapKey) == false) {
    			// It is very rare for this to happen. But, we will check for it.
    			error = KEY_NOT_FOUND_IN_SUB_EXP_MAP_DURING_EVAL;
    			return(false);
    		}

    		// We support nested subexpressions. So, we must first check
    		// if the current map key is part of a nested subexpression group.
    		// In the map keys list, such nested subexpression ids will
    		// appear in a sequential (sorted) order. After finding the first one,
    		// we can process them one by one until we complete all of them.
    		// So, it is necessary to get every new nested subexpression
    		// group information only once. If we are already in the middle of
    		// processing a nested subexpression group, we will skip this check.
    		// Similarly, if a given subexpression is not part of a
    		// nested group, calling the function below will not do any harm.
    		if(intraNestedSubexpressionLogicalOperator == "") {
    			// Let us find out if this map key is part of a
    			// nested subexpression group.
    			getNestedSubexpressionGroupInfo(mapKey,
    				evalPlanPtr->getSubexpressionsMapKeys(),
					evalPlanPtr->getIntraNestedSubexpressionLogicalOperatorsMap(),
					subexpressionCntInCurrentNestedGroup,
					intraNestedSubexpressionLogicalOperator);
    		}

    		// Using the map key, get the subexpression layout list.
    		SPL::list<rstring> const & subexpressionLayoutList =
    			evalPlanPtr->getSubexpressionsMap().at(mapKey);

    		int32 subExpLayoutListCnt =
    			Functions::Collections::size(subexpressionLayoutList);

    		if(subExpLayoutListCnt == 0) {
    			// It is very rare for this to happen. But, we will check for it.
    			error = EMPTY_SUB_EXP_LAYOUT_LIST_DURING_EVAL;
    			return(false);
    		}

			if(trace == true) {
				cout << "==== BEGIN eval_predicate trace 4b ====" << endl;
				cout << "Full expression=" << evalPlanPtr->getExpression() << endl;
				cout << "Subexpressions map key=" << mapKey << endl;
				cout << "subexpressionCntInCurrentNestedGroup=" <<
					subexpressionCntInCurrentNestedGroup << endl;
				cout << "intraNestedSubexpressionLogicalOperator=" <<
					intraNestedSubexpressionLogicalOperator << endl;

				// Print the subexpression layout list during the first iteraion of the while loop.
				cout <<  "Subexpression layout list being evaluated:" << endl;
				ConstListIterator it = subexpressionLayoutList.getBeginIterator();

				while (it != subexpressionLayoutList.getEndIterator()) {
					ConstValueHandle myVal = *it;
					rstring const & myString = myVal;
					cout << myString << endl;
					it++;
				} // End of list iteration while loop.

				cout << "==== END eval_predicate trace 4b ====" << endl;
			}

    		boolean intraSubexpressionEvalResult = false;
    		rstring intraSubexpressionLogicalOperatorInUse = "";
    		int32 loopCnt = 0;
    		int32 idx = 0;

    		// Stay in a loop and evaluate every block available in the subexpression layout list.
    		// We will run the evaluation on multiple dimensions (based on LHS attribute type,
    		// operation verb or a combination of those two. It is plenty of code below to
    		// cover all possible evaluations paths that we can support.
    		while(idx < subExpLayoutListCnt) {
    			loopCnt++;
    			// Get the LHS attribute name.
    			rstring lhsAttributeName = subexpressionLayoutList[idx++];
    			// Get the LHS attribute type.
    			rstring lhsAttributeType = subexpressionLayoutList[idx++];
    			// Get the list index or the map key value.
    			rstring listIndexOrMapKeyValue = subexpressionLayoutList[idx++];
				// Get the operation verb.
    			// For arithmetic verbs, it will have extra stuff. e-g: % 8 ==
				rstring operationVerb = subexpressionLayoutList[idx++];
				rstring arithmeticOperation =
					Functions::String::substring(operationVerb, 0, 1);
				rstring arithmeticOperandValueString = "";
				rstring postArithmeticOperationVerb = "";

				if(arithmeticOperation == "+" ||
					arithmeticOperation == "-" ||
					arithmeticOperation == "*" ||
					arithmeticOperation == "/" ||
					arithmeticOperation == "%") {
					// We have to parse the operation verb that will have extra stuff.
					// e-g: % 8 ==
					SPL::list<rstring> tokens =
						Functions::String::tokenize(operationVerb, " ", false);

					if(Functions::Collections::size(tokens) != 3) {
						// In the arithmetic verb's extra stuff, we must have 3 tokens.
						// e-g: % 8 ==
						error = THREE_TOKENS_NOT_FOUND_IN_ARITHMETIC_OPERATION_VERB;
						return(false);
					}

					// Set the operation verb to just the arithmetic operation symbol.
					operationVerb = arithmeticOperation;
					arithmeticOperandValueString = tokens[1];
					postArithmeticOperationVerb = tokens[2];

					// A safety check that they are not empty which
					// most likely will be the case.
					if(arithmeticOperandValueString == "") {
						error = EMPTY_VALUE_FOUND_FOR_ARITHMETIC_OPERAND;
						return(false);
					}

					if(postArithmeticOperationVerb == "") {
						error = EMPTY_VALUE_FOUND_FOR_POST_ARITHMETIC_OPERATION_VERB;
						return(false);
					}
				} // End of if(arithmeticOperation == "+" ||

				// Get the RHS value.
				rstring rhsValue = subexpressionLayoutList[idx++];
				// Get the intra subexpression logical operator.
				rstring intraSubexpressionLogicalOperator = subexpressionLayoutList[idx++];

				if(loopCnt == 1) {
					// Record the logical operator used within this subexpression if any.
					// They will be homogeneous. So, it is sufficient to record only one of them.
					intraSubexpressionLogicalOperatorInUse = intraSubexpressionLogicalOperator;
				}

				ConstValueHandle cvh;
    			// Get the constant value handle for this attribute.
    			getConstValueHandleForTupleAttribute(myTuple, lhsAttributeName, cvh);
        		boolean subexpressionEvalResult = false;

    			// Depending on the LHS attribute type, operation verb or
    			// a combination of those two, we will perform the evaluations.
    			// ****** rstring evaluations ******
    			if(lhsAttributeType == "rstring") {
    				rstring const & lhsValue = cvh;
    				performRStringEvalOperations(lhsValue, rhsValue,
    					operationVerb, subexpressionEvalResult, error);
    			} else if(lhsAttributeType == "list<rstring>" &&
    				listIndexOrMapKeyValue != "") {
    				SPL::list<rstring> const & myList = cvh;
    				// Check if the user given list index is valid.
    				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 ||
    					listIdx > (Functions::Collections::size(myList) - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				performRStringEvalOperations(myList[listIdx], rhsValue,
    					operationVerb, subexpressionEvalResult, error);
    			} else if(lhsAttributeType == "map<int32,rstring>" &&
        			listIndexOrMapKeyValue != "") {
    				SPL::map<int32,rstring> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				// Using the [] map element access operator with a
    				// const SPL::map variable gives a compiler error
    				// saying that [] operation can't be performed using a const SPL::map.
    				// So, I'm using the at access method.
    				rstring const & lhsValue = myMap.at(mapKey);
    				performRStringEvalOperations(lhsValue, rhsValue,
    					operationVerb, subexpressionEvalResult, error);
    			} else if(lhsAttributeType == "map<int64,rstring>" &&
        			listIndexOrMapKeyValue != "") {
    				SPL::map<int64,rstring> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				// Using the [] map element access operator with a
    				// const SPL::map variable gives a compiler error
    				// saying that [] operation can't be performed using a const SPL::map.
    				// So, I'm using the at access method.
    				rstring const & lhsValue = myMap.at(mapKey);
    				performRStringEvalOperations(lhsValue, rhsValue,
    					operationVerb, subexpressionEvalResult, error);
    			} else if(lhsAttributeType == "map<float32,rstring>" &&
        			listIndexOrMapKeyValue != "") {
    				SPL::map<float32,rstring> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				// The C atof API used above actually returns double which is
    				// equivalent to the SPL float64. So, assigning the result from
    				// that API to an SPL float32 variable can't be made fully
    				// accurate without losing some precision for very large float32 numbers.
    				// It is a known limitation in C. So, for very large float32 based map keys,
    				// I had problems in the SPL 'has' method where it wrongly returned that
    				// the key doesn't exist when it actually is there.
    				// e-g: 5.28438e+08
    				// So, I stopped calling the SPL 'has' function from C++ code.
    				// The following manual procedure by converting the float based key into
    				// a string and then comparing it worked for me. I don't know how much
    				// overhead it will add compared to the SPL 'has' function if it indeed works.
			    	ostringstream key;
			    	key << mapKey;
			    	boolean keyExists = false;

					ConstMapIterator it = myMap.getBeginIterator();

					while (it != myMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
						std::pair<float32,rstring> const & myFloat32RString = myVal;
    			    	ostringstream firstMember;
    			    	firstMember << myFloat32RString.first;

    			    	if(key.str() == firstMember.str()) {
    			    		keyExists = true;
    			    		rstring const & lhsValue = myFloat32RString.second;
    	    				performRStringEvalOperations(lhsValue, rhsValue,
    	    					operationVerb, subexpressionEvalResult, error);
    	    				break;
    			    	}

						it++;
					}

    				if(keyExists == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}
    			} else if(lhsAttributeType == "map<float64,rstring>" &&
        			listIndexOrMapKeyValue != "") {
    				SPL::map<float64,rstring> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				// Using the [] map element access operator with a
    				// const SPL::map variable gives a compiler error
    				// saying that [] operation can't be performed using a const SPL::map.
    				// So, I'm using the at access method.
    				rstring const & lhsValue = myMap.at(mapKey);
    				performRStringEvalOperations(lhsValue, rhsValue,
    					operationVerb, subexpressionEvalResult, error);
    			} else if(lhsAttributeType == "map<rstring,rstring>" &&
        			listIndexOrMapKeyValue != "") {
    				SPL::map<rstring,rstring> const & myMap = cvh;

    				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				// Using the [] map element access operator with a
    				// const SPL::map variable gives a compiler error
    				// saying that [] operation can't be performed using a const SPL::map.
    				// So, I'm using the at access method.
    				rstring const & lhsValue = myMap.at(listIndexOrMapKeyValue);
    				performRStringEvalOperations(lhsValue, rhsValue,
    					operationVerb, subexpressionEvalResult, error);
        		// ****** in membership evaluation for int32 LHS attributes ******
    			} else if(operationVerb == "in" && lhsAttributeType == "int32") {
        			int32 const & myLhsValue = cvh;

        			try {
        				// We can use spl_cast to convert a list string literal into
        				// an SPL list. As long as the string representation truly
        				// reflects an SPL list syntax, this conversion will work.
        				// If not, it will throw an exception.
        				// I learned about this technique by looking at the
        				// auto generated C++ code for a similar conversion done in
        				// a SPL application logic.
        				const SPL::list<SPL::int32> tokens =
        					SPL::spl_cast<SPL::list<SPL::int32>, SPL::rstring>::cast(rhsValue);
        				subexpressionEvalResult =
        					Functions::Collections::has(tokens, myLhsValue);
        			} catch(...) {
        				// SPL type casting of a list string literal to SPL list failed.
        				subexpressionEvalResult = false;
        				error = INVALID_RHS_LIST_LITERAL_STRING_FOUND_FOR_IN_OR_IN_CI_OPVERB;
        			}
            	// ****** in membership evaluation for float64 LHS attributes ******
				} else if(operationVerb == "in" && lhsAttributeType == "float64") {
					float64 const & myLhsValue = cvh;

        			try {
        				// We can use spl_cast to convert a list string literal into
        				// an SPL list. As long as the string representation truly
        				// reflects an SPL list syntax, this conversion will work.
        				// If not, it will throw an exception.
        				// I learned about this technique by looking at the
        				// auto generated C++ code for a similar conversion done in
        				// a SPL application logic.
        				const SPL::list<SPL::float64> tokens =
        					SPL::spl_cast<SPL::list<SPL::float64>, SPL::rstring>::cast(rhsValue);
        				subexpressionEvalResult =
        					Functions::Collections::has(tokens, myLhsValue);
        			} catch(...) {
        				// SPL type casting of a list string literal to SPL list failed.
        				subexpressionEvalResult = false;
        				error = INVALID_RHS_LIST_LITERAL_STRING_FOUND_FOR_IN_OR_IN_CI_OPVERB;
        			}
    			// ****** Collection size check (OR) Collection item existence evaluations ******
    			} else if(lhsAttributeType == "set<int32>") {
    				SPL::set<int32> const & mySet = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(mySet);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(mySet, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "set<int64>") {
    				SPL::set<int64> const & mySet = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(mySet);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(mySet, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "set<float32>") {
    				SPL::set<float32> const & mySet = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(mySet);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(mySet, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "set<float64>") {
    				SPL::set<float64> const & mySet = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(mySet);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(mySet, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "set<rstring>") {
    				SPL::set<rstring> const & mySet = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(mySet);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(mySet, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "list<rstring>" &&
            		listIndexOrMapKeyValue == "") {
    				SPL::list<rstring> const & myList = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myList);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myList, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
    			} else if(lhsAttributeType == "list<int32>" &&
                	listIndexOrMapKeyValue == "") {
        			SPL::list<int32> const & myList = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myList);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myList, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "list<int64>" &&
                	listIndexOrMapKeyValue == "") {
        			SPL::list<int64> const & myList = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myList);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myList, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "list<float32>" &&
        			listIndexOrMapKeyValue == "") {
        			SPL::list<float32> const & myList = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myList);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myList, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "list<float64>" &&
        			listIndexOrMapKeyValue == "") {
        			SPL::list<float64> const & myList = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myList);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myList, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<rstring,rstring>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<rstring,rstring> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myMap, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<rstring,int32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<rstring,int32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myMap, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<rstring,int64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<rstring,int64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myMap, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<rstring,float32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<rstring,float32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myMap, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<rstring,float64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<rstring,float64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						boolean itemExists = Functions::Collections::has(myMap, rhsValue);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int32,rstring>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int32,rstring> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int32,int32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int32,int32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int32,int64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int32,int64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int32,float32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int32,float32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int32,float64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int32,float64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int32 const & item = atoi(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int64,rstring>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int64,rstring> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int64,int32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int64,int32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int64,int64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int64,int64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int64,float32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int64,float32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<int64,float64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<int64,float64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						int64 const & item = atol(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float32,rstring>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float32,rstring> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());

						// The C atof API used above actually returns double which is
						// equivalent to the SPL float64. So, assigning the result from
						// that API to an SPL float32 variable can't be made fully
						// accurate without losing some precision for very large float32 numbers.
						// It is a known limitation in C. So, for very large float32 based map keys,
						// I had problems in the SPL 'has' method where it wrongly returned that
						// the key doesn't exist when it actually is there.
						// e-g: 5.28438e+08
						// So, I stopped calling the SPL 'has' function from C++ code.
						// The following manual procedure by converting the float based key into
						// a string and then comparing it worked for me. I don't know how much
						// overhead it will add compared to the SPL 'has' function if it indeed works.
						ostringstream key;
						key << item;
						boolean itemExists = false;

						ConstMapIterator it = myMap.getBeginIterator();

						while (it != myMap.getEndIterator()) {
							std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
							std::pair<float32,rstring> const & myFloat32RString = myVal;
							ostringstream firstMember;
							firstMember << myFloat32RString.first;

							if(key.str() == firstMember.str()) {
								itemExists = true;
								break;
							}

							it++;
						}

						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float32,int32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float32,int32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());

						// The C atof API used above actually returns double which is
						// equivalent to the SPL float64. So, assigning the result from
						// that API to an SPL float32 variable can't be made fully
						// accurate without losing some precision for very large float32 numbers.
						// It is a known limitation in C. So, for very large float32 based map keys,
						// I had problems in the SPL 'has' method where it wrongly returned that
						// the key doesn't exist when it actually is there.
						// e-g: 5.28438e+08
						// So, I stopped calling the SPL 'has' function from C++ code.
						// The following manual procedure by converting the float based key into
						// a string and then comparing it worked for me. I don't know how much
						// overhead it will add compared to the SPL 'has' function if it indeed works.
						ostringstream key;
						key << item;
						boolean itemExists = false;

						ConstMapIterator it = myMap.getBeginIterator();

						while (it != myMap.getEndIterator()) {
							std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
							std::pair<float32,int32> const & myFloat32Int32 = myVal;
							ostringstream firstMember;
							firstMember << myFloat32Int32.first;

							if(key.str() == firstMember.str()) {
								itemExists = true;
								break;
							}

							it++;
						}

						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float32,int64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float32,int64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());

						// The C atof API used above actually returns double which is
						// equivalent to the SPL float64. So, assigning the result from
						// that API to an SPL float32 variable can't be made fully
						// accurate without losing some precision for very large float32 numbers.
						// It is a known limitation in C. So, for very large float32 based map keys,
						// I had problems in the SPL 'has' method where it wrongly returned that
						// the key doesn't exist when it actually is there.
						// e-g: 5.28438e+08
						// So, I stopped calling the SPL 'has' function from C++ code.
						// The following manual procedure by converting the float based key into
						// a string and then comparing it worked for me. I don't know how much
						// overhead it will add compared to the SPL 'has' function if it indeed works.
						ostringstream key;
						key << item;
						boolean itemExists = false;

						ConstMapIterator it = myMap.getBeginIterator();

						while (it != myMap.getEndIterator()) {
							std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
							std::pair<float32,int64> const & myFloat32Int64 = myVal;
							ostringstream firstMember;
							firstMember << myFloat32Int64.first;

							if(key.str() == firstMember.str()) {
								itemExists = true;
								break;
							}

							it++;
						}

						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float32,float32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float32,float32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());

						// The C atof API used above actually returns double which is
						// equivalent to the SPL float64. So, assigning the result from
						// that API to an SPL float32 variable can't be made fully
						// accurate without losing some precision for very large float32 numbers.
						// It is a known limitation in C. So, for very large float32 based map keys,
						// I had problems in the SPL 'has' method where it wrongly returned that
						// the key doesn't exist when it actually is there.
						// e-g: 5.28438e+08
						// So, I stopped calling the SPL 'has' function from C++ code.
						// The following manual procedure by converting the float based key into
						// a string and then comparing it worked for me. I don't know how much
						// overhead it will add compared to the SPL 'has' function if it indeed works.
						ostringstream key;
						key << item;
						boolean itemExists = false;

						ConstMapIterator it = myMap.getBeginIterator();

						while (it != myMap.getEndIterator()) {
							std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
							std::pair<float32,float32> const & myFloat32Float32 = myVal;
							ostringstream firstMember;
							firstMember << myFloat32Float32.first;

							if(key.str() == firstMember.str()) {
								itemExists = true;
								break;
							}

							it++;
						}

						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float32,float64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float32,float64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float32 const & item = atof(rhsValue.c_str());

						// The C atof API used above actually returns double which is
						// equivalent to the SPL float64. So, assigning the result from
						// that API to an SPL float32 variable can't be made fully
						// accurate without losing some precision for very large float32 numbers.
						// It is a known limitation in C. So, for very large float32 based map keys,
						// I had problems in the SPL 'has' method where it wrongly returned that
						// the key doesn't exist when it actually is there.
						// e-g: 5.28438e+08
						// So, I stopped calling the SPL 'has' function from C++ code.
						// The following manual procedure by converting the float based key into
						// a string and then comparing it worked for me. I don't know how much
						// overhead it will add compared to the SPL 'has' function if it indeed works.
						ostringstream key;
						key << item;
						boolean itemExists = false;

						ConstMapIterator it = myMap.getBeginIterator();

						while (it != myMap.getEndIterator()) {
							std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
							std::pair<float32,float64> const & myFloat32Float64 = myVal;
							ostringstream firstMember;
							firstMember << myFloat32Float64.first;

							if(key.str() == firstMember.str()) {
								itemExists = true;
								break;
							}

							it++;
						}

						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float64,rstring>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float64,rstring> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float64,int32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float64,int32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float64,int64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float64,int64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float64,float32>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float64,float32> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		} else if(lhsAttributeType == "map<float64,float64>" &&
        			listIndexOrMapKeyValue == "") {
        			// For maps, we only support key existence check.
        			SPL::map<float64,float64> const & myMap = cvh;

    				if(Functions::String::findFirst(operationVerb, "size") == 0) {
    					int32 lhsSize = Functions::Collections::size(myMap);
    					int32 rhsInt32 = atoi(rhsValue.c_str());
    					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
    						operationVerb, subexpressionEvalResult, error);
    				} else {
						float64 const & item = atof(rhsValue.c_str());
						boolean itemExists = Functions::Collections::has(myMap, item);
						performCollectionItemExistenceEvalOperations(itemExists, operationVerb,
							subexpressionEvalResult, error);
    				}
        		// ****** Relational or arithmetic evaluations ******
        		} else if(lhsAttributeType == "int32" &&
        			(operationVerb == "==" || operationVerb == "!=" ||
        			operationVerb == "<" || operationVerb == "<=" ||
					operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			int32 const & myLhsValue = cvh;
        			int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "uint32" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			uint32 const & myLhsValue = cvh;
        			uint32 const & myRhsValue = atoi(rhsValue.c_str());
        			uint32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "int64" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			int64 const & myLhsValue = cvh;
        			int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "uint64" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			uint64 const & myLhsValue = cvh;
        			uint64 const & myRhsValue = atol(rhsValue.c_str());
        			uint64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "float32" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			float32 const & myLhsValue = cvh;
        			float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "float64" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
        			float64 const & myLhsValue = cvh;
        			float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "boolean" &&
            		(operationVerb == "==" || operationVerb == "!=")) {
        			boolean const & myLhsValue = cvh;
        			boolean myRhsValue = false;

        			if(rhsValue == "true") {
        				myRhsValue = true;
        			}

        			// A dummy one since there is no arithmetic operation
        			// possible for this LHS attribute type.
        			boolean arithmeticOperandValue = false;
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "list<int32>" &&
        			(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::list<int32> const & myList = cvh;
    				// Check if the user given list index is valid.
    				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 ||
    					listIdx > (Functions::Collections::size(myList) - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				int32 const & myLhsValue = myList[listIdx];
    				int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "list<int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::list<int64> const & myList = cvh;
    				// Check if the user given list index is valid.
    				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 ||
    					listIdx > (Functions::Collections::size(myList) - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				int64 const & myLhsValue = myList[listIdx];
    				int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "list<float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::list<float32> const & myList = cvh;
    				// Check if the user given list index is valid.
    				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 ||
    					listIdx > (Functions::Collections::size(myList) - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				float32 const & myLhsValue = myList[listIdx];
    				float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "list<float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::list<float64> const & myList = cvh;
    				// Check if the user given list index is valid.
    				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 ||
    					listIdx > (Functions::Collections::size(myList) - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				float64 const & myLhsValue = myList[listIdx];
    				float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<rstring,int32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<rstring,int32> const & myMap = cvh;

    				// Check if the user given map key is valid.
    				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int32 const & myLhsValue = myMap.at(listIndexOrMapKeyValue);
    				int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<rstring,int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<rstring,int64> const & myMap = cvh;

    				// Check if the user given map key is valid.
    				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int64 const & myLhsValue = myMap.at(listIndexOrMapKeyValue);
    				int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<rstring,float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<rstring,float32> const & myMap = cvh;

    				// Check if the user given map key is valid.
    				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float32 const & myLhsValue = myMap.at(listIndexOrMapKeyValue);
    				float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<rstring,float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<rstring,float64> const & myMap = cvh;

    				// Check if the user given map key is valid.
    				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float64 const & myLhsValue = myMap.at(listIndexOrMapKeyValue);
    				float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int32,int32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int32,int32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int32 const & myLhsValue = myMap.at(mapKey);
    				int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int32,int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int32,int64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int64 const & myLhsValue = myMap.at(mapKey);
    				int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int64,int32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int64,int32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int32 const & myLhsValue = myMap.at(mapKey);
    				int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int64,int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int64,int64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int64 const & myLhsValue = myMap.at(mapKey);
    				int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int32,float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int32,float32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float32 const & myLhsValue = myMap.at(mapKey);
    				float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int32,float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int32,float64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float64 const & myLhsValue = myMap.at(mapKey);
    				float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int64,float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int64,float32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float32 const & myLhsValue = myMap.at(mapKey);
    				float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<int64,float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<int64,float64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float64 const & myLhsValue = myMap.at(mapKey);
    				float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<float32,int32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float32,int32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				// The C atof API used above actually returns double which is
    				// equivalent to the SPL float64. So, assigning the result from
    				// that API to an SPL float32 variable can't be made fully
    				// accurate without losing some precision for very large float32 numbers.
    				// It is a known limitation in C. So, for very large float32 based map keys,
    				// I had problems in the SPL 'has' method where it wrongly returned that
    				// the key doesn't exist when it actually is there.
    				// e-g: 5.28438e+08
    				// So, I stopped calling the SPL 'has' function from C++ code.
    				// The following manual procedure by converting the float based key into
    				// a string and then comparing it worked for me. I don't know how much
    				// overhead it will add compared to the SPL 'has' function if it indeed works.
			    	ostringstream key;
			    	key << mapKey;
			    	boolean keyExists = false;

					ConstMapIterator it = myMap.getBeginIterator();

					while (it != myMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
						std::pair<float32,int32> const & myFloat32Int32 = myVal;
    			    	ostringstream firstMember;
    			    	firstMember << myFloat32Int32.first;

    			    	if(key.str() == firstMember.str()) {
    			    		keyExists = true;
    			    		int32 const & myLhsValue = myFloat32Int32.second;
    	    				int32 const & myRhsValue = atoi(rhsValue.c_str());
    	        			int32 arithmeticOperandValue =
    	        				atoi(arithmeticOperandValueString.c_str());
    	        			performRelationalOrArithmeticEvalOperations(myLhsValue,
    	        				myRhsValue, operationVerb,
    							arithmeticOperandValue,
    							postArithmeticOperationVerb,
    							subexpressionEvalResult, error);
    	    				break;
    			    	}

						it++;
					}

    				if(keyExists == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}
        		} else if(lhsAttributeType == "map<float32,int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float32,int64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				// The C atof API used above actually returns double which is
    				// equivalent to the SPL float64. So, assigning the result from
    				// that API to an SPL float32 variable can't be made fully
    				// accurate without losing some precision for very large float32 numbers.
    				// It is a known limitation in C. So, for very large float32 based map keys,
    				// I had problems in the SPL 'has' method where it wrongly returned that
    				// the key doesn't exist when it actually is there.
    				// e-g: 5.28438e+08
    				// So, I stopped calling the SPL 'has' function from C++ code.
    				// The following manual procedure by converting the float based key into
    				// a string and then comparing it worked for me. I don't know how much
    				// overhead it will add compared to the SPL 'has' function if it indeed works.
			    	ostringstream key;
			    	key << mapKey;
			    	boolean keyExists = false;

					ConstMapIterator it = myMap.getBeginIterator();

					while (it != myMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
						std::pair<float32,int64> const & myFloat32Int64 = myVal;
    			    	ostringstream firstMember;
    			    	firstMember << myFloat32Int64.first;

    			    	if(key.str() == firstMember.str()) {
    			    		keyExists = true;
    			    		int64 const & myLhsValue = myFloat32Int64.second;
    	    				int64 const & myRhsValue = atol(rhsValue.c_str());
    	        			int64 arithmeticOperandValue =
    	        				atol(arithmeticOperandValueString.c_str());
    	        			performRelationalOrArithmeticEvalOperations(myLhsValue,
    	        				myRhsValue, operationVerb,
    							arithmeticOperandValue,
    							postArithmeticOperationVerb,
    							subexpressionEvalResult, error);
    			    		break;
    			    	}

						it++;
					}

    				if(keyExists == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}
        		} else if(lhsAttributeType == "map<float64,int32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float64,int32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int32 const & myLhsValue = myMap.at(mapKey);
    				int32 const & myRhsValue = atoi(rhsValue.c_str());
        			int32 arithmeticOperandValue =
        				atoi(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<float64,int64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float64,int64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				int64 const & myLhsValue = myMap.at(mapKey);
    				int64 const & myRhsValue = atol(rhsValue.c_str());
        			int64 arithmeticOperandValue =
        				atol(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<float32,float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float32,float32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				// The C atof API used above actually returns double which is
    				// equivalent to the SPL float64. So, assigning the result from
    				// that API to an SPL float32 variable can't be made fully
    				// accurate without losing some precision for very large float32 numbers.
    				// It is a known limitation in C. So, for very large float32 based map keys,
    				// I had problems in the SPL 'has' method where it wrongly returned that
    				// the key doesn't exist when it actually is there.
    				// e-g: 5.28438e+08
    				// So, I stopped calling the SPL 'has' function from C++ code.
    				// The following manual procedure by converting the float based key into
    				// a string and then comparing it worked for me. I don't know how much
    				// overhead it will add compared to the SPL 'has' function if it indeed works.
			    	ostringstream key;
			    	key << mapKey;
			    	boolean keyExists = false;

					ConstMapIterator it = myMap.getBeginIterator();

					while (it != myMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
						std::pair<float32,float32> const & myFloat32Float32 = myVal;
    			    	ostringstream firstMember;
    			    	firstMember << myFloat32Float32.first;

    			    	if(key.str() == firstMember.str()) {
    			    		keyExists = true;
    			    		float32 const & myLhsValue = myFloat32Float32.second;
    	    				float32 const & myRhsValue = atof(rhsValue.c_str());
    	        			float32 arithmeticOperandValue =
    	        				atof(arithmeticOperandValueString.c_str());
    	        			performRelationalOrArithmeticEvalOperations(myLhsValue,
    	        				myRhsValue, operationVerb,
    							arithmeticOperandValue,
    							postArithmeticOperationVerb,
    							subexpressionEvalResult, error);
    			    		break;
    			    	}

						it++;
					}

    				if(keyExists == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}
        		} else if(lhsAttributeType == "map<float32,float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float32,float64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				// The C atof API used above actually returns double which is
    				// equivalent to the SPL float64. So, assigning the result from
    				// that API to an SPL float32 variable can't be made fully
    				// accurate without losing some precision for very large float32 numbers.
    				// It is a known limitation in C. So, for very large float32 based map keys,
    				// I had problems in the SPL 'has' method where it wrongly returned that
    				// the key doesn't exist when it actually is there.
    				// e-g: 5.28438e+08
    				// So, I stopped calling the SPL 'has' function from C++ code.
    				// The following manual procedure by converting the float based key into
    				// a string and then comparing it worked for me. I don't know how much
    				// overhead it will add compared to the SPL 'has' function if it indeed works.
			    	ostringstream key;
			    	key << mapKey;
			    	boolean keyExists = false;

					ConstMapIterator it = myMap.getBeginIterator();

					while (it != myMap.getEndIterator()) {
						std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
						std::pair<float32,float64> const & myFloat32Float64 = myVal;
    			    	ostringstream firstMember;
    			    	firstMember << myFloat32Float64.first;

    			    	if(key.str() == firstMember.str()) {
    			    		keyExists = true;
    			    		float64 const & myLhsValue = myFloat32Float64.second;
    	    				float64 const & myRhsValue = atof(rhsValue.c_str());
    	        			float64 arithmeticOperandValue =
    	        				atof(arithmeticOperandValueString.c_str());
    	        			performRelationalOrArithmeticEvalOperations(myLhsValue,
    	        				myRhsValue, operationVerb,
    							arithmeticOperandValue,
    							postArithmeticOperationVerb,
    							subexpressionEvalResult, error);
    			    		break;
    			    	}

						it++;
					}

    				if(keyExists == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}
        		} else if(lhsAttributeType == "map<float64,float32>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float64,float32> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float32 const & myLhsValue = myMap.at(mapKey);
    				float32 const & myRhsValue = atof(rhsValue.c_str());
        			float32 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        		} else if(lhsAttributeType == "map<float64,float64>" &&
            		(operationVerb == "==" || operationVerb == "!=" ||
            		operationVerb == "<" || operationVerb == "<=" ||
    				operationVerb == ">" || operationVerb == ">=" ||
					operationVerb == "+" || operationVerb == "-" ||
					operationVerb == "*" || operationVerb == "/" ||
					operationVerb == "%")) {
    				SPL::map<float64,float64> const & myMap = cvh;
    				// Check if the user given map key is valid.
    				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

    				if(Functions::Collections::has(myMap, mapKey) == false) {
    					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
    					return(false);
    				}

    				float64 const & myLhsValue = myMap.at(mapKey);
    				float64 const & myRhsValue = atof(rhsValue.c_str());
        			float64 arithmeticOperandValue =
        				atof(arithmeticOperandValueString.c_str());
        			performRelationalOrArithmeticEvalOperations(myLhsValue,
        				myRhsValue, operationVerb,
						arithmeticOperandValue,
						postArithmeticOperationVerb,
						subexpressionEvalResult, error);
        			// ****** list<TUPLE> evaluations ******
        		} else if(Functions::String::findFirst(lhsAttributeType, "list<tuple<") == 0  &&
        			listIndexOrMapKeyValue == "") {
        			// This is a list<TUPLE> with no index value.
        			// Then, it must be for checking the list size.
    				// We must use the C++ interface type SPL::List that the
    				// SPL::list is based on.
    				// The line below shows an example of an attribute type (schema)
    				// followed by an attribute name.
    				// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList
        			SPL::List const & myListTuple = cvh;
					int32 lhsSize = myListTuple.getSize();
					int32 rhsInt32 = atoi(rhsValue.c_str());
					performCollectionSizeCheckEvalOperations(lhsSize, rhsInt32,
						operationVerb, subexpressionEvalResult, error);
        		} else if(Functions::String::findFirst(lhsAttributeType, "list<tuple<") == 0  &&
        			listIndexOrMapKeyValue != "") {
					// If it is a list<TUPLE> with an index value, this needs a special evaluation.
					// e-g: Body.MachineStatus.ComponentList[0].Component["MapKey"] == "MapValue"
    				// This list doesn't hold items that are made of
        			// well defined SPL built-in data types.
    				// Instead, it holds a user defined Tuple type. So, we can't
    				// adopt the technique we used in the other else blocks above
    				// to assign cvh to a variable such as SPL::list<rstring>.
    				// We must use the C++ interface type SPL::List that the
    				// SPL::list is based on.
    				// The line below shows an example of an attribute type (schema)
    				// followed by an attribute name.
    				// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList
        			SPL::List const & myListTuple = cvh;
        			ConstListIterator it = myListTuple.getBeginIterator();

        			// We have to look only for a tuple that is present in the
        			// user specified list index. Let us first check if that
        			// index is valid in a given list.
        			int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

    				if(listIdx < 0 || listIdx > (myListTuple.getSize() - 1)) {
    					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
    					return(false);
    				}

    				// Use of "lot" in the local variable names below means List Of Tuple.
    				int32 lotIdx = -1;
    				boolean lotResult = false;
    				SPL::map<rstring, rstring> lotTupleAttributesMap;
    				int32 lotError = 0;
    				rstring lotTupleSchema = "";

        			while (it != myListTuple.getEndIterator()) {
        				// We have to evaluate only the tuple held in a
        				// user given list index value in the expression.
        				// Remaining list items can be skipped.
        				lotIdx++;

        				if(lotIdx != listIdx) {
        					// Continue the while loop.
        					it++;
        					continue;
        				}

        				ConstValueHandle myVal = *it;
        				Tuple const & lotTuple = myVal;

        				// We can now get the attributes present inside this tuple.
        				//
        				int32 lotSchemaLength =
        					Functions::String::length(lhsAttributeType);

        				// Get just the tuple<...> part from this attribute's type.
        				// We have to skip the initial "list<" portion in the type string.
        				// We also have to skip the final > in the type string that
        				// is a closure for "list<" i.e. we start from the
        				// zero based index 5 and take the substring until the
        				// end except for the final > which in total is 6 characters
        				// less than the length of the entire type string.
        				lotTupleSchema = Functions::String::substring(lhsAttributeType,
        					5, lotSchemaLength-6);
        				lotResult = parseTupleAttributes(lotTupleSchema,
        					lotTupleAttributesMap, lotError, trace);

        				if(lotResult == false) {
        					// This error should never happen since we
        					// already passed this check twice even before
        					// coming into this evaluation method.
        					// We are simply doing it here as a safety measure.
        					error = ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_EVALUATION;

        					if(trace == true) {
    	    					cout << "It failed to get the list<TUPLE> attributes for " <<
    	    						lhsAttributeName <<
    								" during expression evaluation. Error=" << lotError <<
    								". Tuple schema=" << lotTupleSchema << endl;
        					}

        					// Stop the evaluation of list<TUPLE> due to this rare error.
        					break;
        				}

						// We got the LOT tuple attributes.
        				// At this time, we have to get the subexpression map for
        				// the subexpression involving a list<TUPLE>. Unlike we
        				// normally do it in the validation method for the
        				// SPL built-in data types based subexpressions,
        				// we don't prepare it for a list<TUPLE> based
        				// subexpression and save it ahead of time in the
        				// expression cache. So, we now have to make a straight
        				// non-recursive call to the the validation method which
        				// will get us a subexpression map that we can use
        				// here only once for evaluating a list<TUPLE> based
        				// subexpression.
        				//
        				// We have to parse just a partial portion of the full
        				// subexpression string that is stored in the expression cache.
        				// In the normal expression validation step earlier,
        				// we have stored the start index and end index for
        				// that partial string portion we need below.
        				// You can refer back to the validation method to see
        				// how we store these two indices. In essence,
        				// operationVerb and rhsValue entries of the SELOL
        				// already contain these two numbers in the case of a list<TUPLE>.
        				int32 startIdx = atoi(operationVerb.c_str());
        				int32 endIdx = atoi(rhsValue.c_str());
        				rstring lotSubexpression = evalPlanPtr->getExpression();
        				// We can take a substring to get the
        				// subexpression portion that we are interested in.
        				lotSubexpression = Functions::String::substring(
        					lotSubexpression, startIdx, (endIdx-startIdx+1));

        				// In the LOT subexpression, we can't have any open or
        				// close parenthesis. In a partially taken subexpression,
        				// that situation will cause parenthesis mismatch and
        				// it will get rejected in the validation method.
        				// So, if the very last character of the substring is
        				// a close parenthesis, we will remove it now.
        				// In general, after and RHS value in a given
        				// expression string, we usually have these possible
        				// characters: nothing or space or )
        				//
        				int32 subexpLength = Functions::String::length(lotSubexpression);

        				if(Functions::String::findFirst(lotSubexpression, ")",
        					subexpLength-1) == subexpLength-1) {
        					// We have to exclude the close parenthesis.
        					lotSubexpression = Functions::String::substring(
        						lotSubexpression, 0, subexpLength-1);
        				}

        				if(trace == true) {
        					cout << "LOT subexpression in eval=" << lotSubexpression << endl;
        				}

						SPL::map<rstring, SPL::list<rstring> > lotSubexpressionsMap;
						SPL::map<rstring, rstring> lotIntraNestedSubexpressionLogicalOperatorsMap;
						SPL::list<rstring> lotInterSubexpressionLogicalOperatorsList;
						// We are making a non-recursive call. So, set it to 0.
						// i.e. start validating from index 0 of the
						// subexpression string we created above using substring.
						int32 validationStartIdx = 0;
						lotResult = validateExpression(lotSubexpression, lotTupleAttributesMap,
							lotSubexpressionsMap, lotIntraNestedSubexpressionLogicalOperatorsMap,
							lotInterSubexpressionLogicalOperatorsList, error,
							validationStartIdx, trace);

						if(lotResult == false) {
							// There was a validation error.
							// This should be rare as we have done this successfully
							// once already in the normal validation step earlier.
							if(trace == true) {
								cout << "LOT validation error in eval. Error=" << error << endl;
							}

							break;
						}

						// We can recursively call the current
						// method that we are in now to evaluate the
						// tuple attribute access involving a list<TUPLE>.
						if(trace == true) {
							cout << "BEGIN Recursive evaluate expression call for list<TUPLE> " <<
								lhsAttributeName << "." << endl;
						}

						SPL::list<rstring> lotSubexpressionsMapKeys =
							Functions::Collections::keys(lotSubexpressionsMap);
						Functions::Collections::sortM(lotSubexpressionsMapKeys);

						// We can now create a new eval plan for this
						// special expression involving a list<TUPLE>.
						ExpressionEvaluationPlan *lotEvalPlanPtr = NULL;
						lotEvalPlanPtr = new ExpressionEvaluationPlan();

						if(lotEvalPlanPtr == NULL) {
							error = EXP_EVAL_PLAN_OBJECT_CREATION_ERROR_FOR_LIST_OF_TUPLE;
							break;
						}

						// Let us take a copy of various data structures related to this
						// fully validated expression in our eval plan cache for
						// prolonged use by calling the setter methods of the cache object.
						lotEvalPlanPtr->setExpression(lotSubexpression);
						lotEvalPlanPtr->setTupleSchema(lotTupleSchema);
						lotEvalPlanPtr->setSubexpressionsMap(lotSubexpressionsMap);
						lotEvalPlanPtr->setSubexpressionsMapKeys(lotSubexpressionsMapKeys);
						lotEvalPlanPtr->setIntraNestedSubexpressionLogicalOperatorsMap(
							lotIntraNestedSubexpressionLogicalOperatorsMap);
						lotEvalPlanPtr->setInterSubexpressionLogicalOperatorsList(
							lotInterSubexpressionLogicalOperatorsList);

						subexpressionEvalResult =
							evaluateExpression(lotEvalPlanPtr, lotTuple, error, trace);

						if(trace == true) {
							cout << "END Recursive evaluate expression call for list<TUPLE> " <<
								lhsAttributeName << "." << endl;
						}

						// We are done evaluating the single tuple from this
						// list<TUPLE> that the user specified in the expression.
						delete lotEvalPlanPtr;
						break;
        			} // End of while loop
        		} else {
        			// Unsupported evaluation condition.
        			error = UNSUPPORTED_EVAL_CONDITION_DETECTED;
        		} // End of the many if conditional blocks for different eval checks.

    			// Check the evaluation error status.
    			if(error != ALL_CLEAR) {
    				return(false);
    			}

    			boolean skipRemainingEvals = false;

    			if(loopCnt == 1) {
    				// This is the very first eval block that we completed
    				// within the current subexpression layout list.
    				// Let us accumulate the eval results starting from this one.
    				intraSubexpressionEvalResult = subexpressionEvalResult;

    				// We may be able to bail out from evaluating the
    				// remaining parts of the expression by doing this
    				// simple optimization in the very first part of
    				// a given subexpression.
    				if(intraSubexpressionLogicalOperatorInUse == "&&") {
    					if(intraSubexpressionEvalResult == false) {
    						// This is a logical AND. If it is already false,
    						// we can optimize by skipping the rest of the
    						// evals remaining in this subexpression layout list.
    						skipRemainingEvals = true;
    					}
    				} else if(intraSubexpressionLogicalOperatorInUse == "||") {
    					if(intraSubexpressionEvalResult == true) {
    						// This is a logical OR. If it is already true,
    						// we can optimize by skipping the rest of the
    						// evals remaining in this subexpression layout list.
    						skipRemainingEvals = true;
    					}
    				}
    			} else {
    				// This is a successive eval result within a
    				// multi-part subexpression. We can combine it
    				// with the accumulated eval results thus far.
    				if(intraSubexpressionLogicalOperatorInUse == "&&") {
    					intraSubexpressionEvalResult =
    						intraSubexpressionEvalResult && subexpressionEvalResult;

    					if(intraSubexpressionEvalResult == false) {
    						// This is a logical AND. If it is already false,
    						// we can optimize by skipping the rest of the
    						// evals remaining in this subexpression layout list.
    						skipRemainingEvals = true;
    					}
    				} else {
    					intraSubexpressionEvalResult =
    						intraSubexpressionEvalResult || subexpressionEvalResult;

    					if(intraSubexpressionEvalResult == true) {
    						// This is a logical OR. If it is already true,
    						// we can optimize by skipping the rest of the
    						// evals remaining in this subexpression layout list.
    						skipRemainingEvals = true;
    					}
    				}
    			}

    			if(trace == true) {
    				cout << "==== BEGIN eval_predicate trace 4c ====" << endl;
    				cout << "Full expression=" << evalPlanPtr->getExpression() << endl;
    				cout << "Subexpressions map key=" << mapKey << endl;
    				cout << "subexpressionCntInCurrentNestedGroup=" <<
    					subexpressionCntInCurrentNestedGroup << endl;
    				cout << "intraNestedSubexpressionLogicalOperator=" <<
    					intraNestedSubexpressionLogicalOperator << endl;

    				cout << "Loop Count=" << loopCnt << endl;
    				cout << "intraSubexpressionLogicalOperatorInUse=" <<
    					intraSubexpressionLogicalOperatorInUse << endl;
    				cout << "subexpressionEvalResult=" << subexpressionEvalResult << endl;
    				cout << "intraSubexpressionEvalResult=" << intraSubexpressionEvalResult << endl;
    				cout << "skipRemainingEvals=" << skipRemainingEvals << endl;
    				cout << "==== END eval_predicate trace 4c ====" << endl;
    			}

    			// Let us see if we reached the end of the subexpression layout list or
    			// optimize by skipping the rest of the evals.
    			if(skipRemainingEvals == true ||
    				intraSubexpressionLogicalOperator == "") {
    				// We are done evaluating every block within this subexpression layout list.
    				break;
    			}
    		} // End of while(true)

    		// If this subexpression is not part of a nested group, we can
    		// add its eval result right away in our interSubexpressionEvalResults list.
    		if(intraNestedSubexpressionLogicalOperator == "") {
    			// This is not part of a nested group.
				// Keep appending its eval result to the list where we
    			// store all the inter subexpression eval results.
				Functions::Collections::appendM(interSubexpressionEvalResults,
					intraSubexpressionEvalResult);
				// Continue the for loop.
				continue;
    		}

    		// We have a nested subexpression here. If we are in the middle of
    		// processing subexpressions in a nested group, then we have to
    		// collect such individual evaluation results. After we complete
    		// the entire nested group, then we can combine those results and
    		// add a single consolidated result for that nested group in our
    		// interSubexpressionEvalResults list.
    		//
    		// Add the result to the nested subexpression eval result list.
    		Functions::Collections::appendM(nestedSubexpressionEvalResults,
    			intraSubexpressionEvalResult);
    		// Decrement the nested subexpression counter and see if we
    		// processed all the subexpressions in that nested group.
    		subexpressionCntInCurrentNestedGroup--;

    		if(subexpressionCntInCurrentNestedGroup > 0) {
    			// We are not yet done evaluating everything in this nested group.
    			// Continue the next map key iteration in the for loop.
    			continue;
    		}

			// We are done evaluating this nested group.
			// We can consolidate its eval results now.
			int32 listSize =
				Functions::Collections::size(nestedSubexpressionEvalResults);
			// Get the first eval result from the list.
			boolean nestedEvalResult = nestedSubexpressionEvalResults[0];

			for(int32 x=1; x<listSize; x++) {
				if(intraNestedSubexpressionLogicalOperator == "&&") {
					nestedEvalResult = nestedEvalResult && nestedSubexpressionEvalResults[x];

					// Minor optimization by which we can break from the
					// for loop if the logical AND result is false.
					if(nestedEvalResult == false) {
						break;
					}
				} else {
					nestedEvalResult = nestedEvalResult || nestedSubexpressionEvalResults[x];

					// Minor optimization by which we can break from the
					// for loop if the logical OR result is true.
					if(nestedEvalResult == true) {
						break;
					}
				}
			} // End of for loop.

			// We can now add the consolidated result from this
			// nested group to our interSubexpressionEvalResults list.
			Functions::Collections::appendM(interSubexpressionEvalResults,
				nestedEvalResult);
			// We are done with this particular nested subexpression group.
			// We can reset the important local variables we used for that.
			subexpressionCntInCurrentNestedGroup = 0;
			intraNestedSubexpressionLogicalOperator = "";
			Functions::Collections::clearM(nestedSubexpressionEvalResults);
    	} // End of the for loop iterating over the subexpression map keys.

    	int32 numberOfEvalResults = Functions::Collections::size(interSubexpressionEvalResults);
    	// Start with the very first eval result.
    	boolean finalEvalResult = interSubexpressionEvalResults[0];

    	// Starting from the second result, perform the cumulative
    	// logical operation of all the available results.
    	for(int i=1; i<numberOfEvalResults; i++) {
    		// Take the next inter subexpression logical operator.
    		rstring logicalOperator =
    			evalPlanPtr->getInterSubexpressionLogicalOperatorsList()[i-1];

    		// Perform the inter subexpression logical operation.
    		if(logicalOperator == "&&") {
    			finalEvalResult = finalEvalResult && interSubexpressionEvalResults[i];

    			// Minor optimization by which we can break from the
    			// for loop if the logical AND result is false.
    			if(finalEvalResult == false) {
    				break;
    			}
    		} else {
    			finalEvalResult = finalEvalResult || interSubexpressionEvalResults[i];

    			// Minor optimization by which we can break from the
    			// for loop if the logical OR result is true.
    			if(finalEvalResult == true) {
    				break;
    			}
    		}
    	}

		if(trace == true) {
			cout << "==== BEGIN eval_predicate trace 4d ====" << endl;
			cout << "Full expression=" << evalPlanPtr->getExpression() << endl;

			cout <<  "Inter subexpression eval results list after evaluating the full expression." << endl;
			ConstListIterator it1 = interSubexpressionEvalResults.getBeginIterator();

			while (it1 != interSubexpressionEvalResults.getEndIterator()) {
				ConstValueHandle myVal1 = *it1;
				boolean const & myBoolean1 = myVal1;
				cout << myBoolean1 << endl;
				it1++;
			}

			cout <<  "Intra nested subexpression logical operators map after evaluating the full expression." << endl;
			SPL::list<rstring> nestedSubexpressionIds =
				Functions::Collections::keys(evalPlanPtr->getIntraNestedSubexpressionLogicalOperatorsMap());
			Functions::Collections::sortM(nestedSubexpressionIds);
			int32 nestedSubexpressionIdsListSize =
				Functions::Collections::size(nestedSubexpressionIds);

			for(int32 i=0; i<nestedSubexpressionIdsListSize; i++) {
				rstring idString = nestedSubexpressionIds[i];
				rstring currentLogicalOperator =
					evalPlanPtr->getIntraNestedSubexpressionLogicalOperatorsMap().at(idString);
				cout << "Subexpression id=" << idString <<
					", Logical operator=" << currentLogicalOperator << endl;
			} // End of for loop.

			cout <<  "Inter subexpression logical operators list after evaluating the full expression." << endl;
			ConstListIterator it2 =
				evalPlanPtr->getInterSubexpressionLogicalOperatorsList().getBeginIterator();

			while (it2 != evalPlanPtr->getInterSubexpressionLogicalOperatorsList().getEndIterator()) {
				ConstValueHandle myVal2 = *it2;
				rstring const & myString2 = myVal2;
				cout << myString2 << endl;
				it2++;
			}

			cout << "Final eval result=" << finalEvalResult << endl;
			cout << "==== END eval_predicate trace 4d ====" << endl;
		}

    	return(finalEvalResult);
    } //  End of evaluateExpression.
    // ====================================================================

    // ====================================================================
    // This section contains several utility functions useful for
    // validating and evaluating a given expression.
    // They perform tasks such as the ones to fetch the value of
    // a given tuple attribute, carry out specific eval operations,
    // calculate subexpression id and more.
    //
    // This function returns the constant value handle of a given
    // attribute name present inside a given tuple.
    inline void getConstValueHandleForTupleAttribute(Tuple const & myTuple,
    	rstring attributeName, ConstValueHandle & cvh) {
		// This attribute may be inside a nested tuple. So, let us parse and
		// get all the nested tuple names that are separated by a period character.
		SPL::list<rstring> attribTokens =
			Functions::String::tokenize(attributeName, ".", false);

		if(Functions::Collections::size(attribTokens) == 1) {
			// This is just an attribute that is not in a nested tuple and
			// instead is part of the top level tuple.
			cvh = myTuple.getAttributeValue(attributeName);
		} else {
			// Let us traverse through all the nested tuples and
			// reach the final tuple in the nested hierarchy for the current attribute.
			// Start with with the very first tuple i.e. at index 0 in the
			// list that contains all the nested tuple names.
			cvh = myTuple.getAttributeValue(attribTokens[0]);

			// Loop through the list and reach the final tuple in the
			// nested hierarchy i.e. N-1 where the Nth one is the actual attribute.
			for(int j=1; j<Functions::Collections::size(attribTokens)-1; j++) {
				Tuple const & data1 = cvh;
				cvh = data1.getAttributeValue(attribTokens[j]);
			}

			// Now that we reached the last tuple in the nested hierarchy,
			// we can read the value of the actual attribute inside that tuple.
			// e-g: details.location.geo.latitude
			// In this example, geo is the last tuple in the nested hierarchy and
			// it contains the latitude attribute.
			Tuple const & data2 = cvh;
			cvh = data2.getAttributeValue(attribTokens[Functions::Collections::size(attribTokens)-1]);
		} // End of else block.
	} // End of getConstValueHandleForTupleAttribute

    // This function performs the eval operations for rstring based attributes.
    inline void performRStringEvalOperations(rstring const & lhsValue,
    	rstring const & rhsValue, rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error) {
    	error = ALL_CLEAR;

    	// Check if we have a period character present in the
    	// LHS and/or RHS values. This will come handy in the
    	// string based relational operation logic that is
    	// done at the bottom of this method.
    	boolean isLhsValueString = false;
    	boolean isRhsValueString = false;
    	boolean isLhsValueFloat = false;
    	boolean isRhsValueFloat = false;
		int32 lhsInt32 = 0;
		int32 rhsInt32 = 0;
		float64 lhsFloat64 = 0.0;
		float64 rhsFloat64 = 0.0;

		// Find out if a given string represents a string or a number.
		isLhsValueString = (isNumber(lhsValue) == true ? false : true);
		isRhsValueString = (isNumber(rhsValue) == true ? false : true);

		// Find out if LHS represents a float number.
    	if(isLhsValueString == false &&
    		Functions::String::findFirst(lhsValue, ".") != -1) {
    		isLhsValueFloat = true;
    	}

    	// Find out if RHS represents a float number.
    	if(isRhsValueString == false &&
    		Functions::String::findFirst(rhsValue, ".") != -1) {
    		isRhsValueFloat = true;
    	}

		// Allowed operations for rstring are these:
		// ==, !=, contains, notContains, startsWith,
		// notStartsWith, endsWith, notEndsWith, in,
        // containsCI, startsWithCI, endsWithCI, inCI, equalsCI,
    	// notContainsCI, notStartsWithCI, notEndsWithCI, notEqualsCI, sizeXX
		if(operationVerb == "==") {
			subexpressionEvalResult = (lhsValue == rhsValue) ? true : false;
		} else if(operationVerb == "!=") {
			subexpressionEvalResult = (lhsValue != rhsValue) ? true : false;
		} else if(operationVerb == "contains") {
			if(Functions::String::findFirst(lhsValue, rhsValue) != -1) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "notContains") {
			if(Functions::String::findFirst(lhsValue, rhsValue) == -1) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "startsWith") {
			if(Functions::String::findFirst(lhsValue, rhsValue) == 0) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "notStartsWith") {
			if(Functions::String::findFirst(lhsValue, rhsValue) != 0) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "endsWith") {
			int32 lhsLen = Functions::String::length(lhsValue);
			int32 rhsLen = Functions::String::length(rhsValue);

			if(lhsLen < rhsLen) {
				subexpressionEvalResult = false;
			} else if(lhsLen == rhsLen) {
				if(lhsValue == rhsValue) {
					subexpressionEvalResult = true;
				} else {
					subexpressionEvalResult = false;
				}
			} else {
				// e-g: LHS="Happy", RHS="ppy"
				rstring str = Functions::String::substring(lhsValue,
					lhsLen-rhsLen, rhsLen);

				if(str == rhsValue) {
					subexpressionEvalResult = true;
				} else {
					subexpressionEvalResult = false;
				}
			}
		} else if(operationVerb == "notEndsWith") {
			int32 lhsLen = Functions::String::length(lhsValue);
			int32 rhsLen = Functions::String::length(rhsValue);

			if(lhsLen < rhsLen) {
				subexpressionEvalResult = true;
			} else if(lhsLen == rhsLen) {
				if(lhsValue == rhsValue) {
					subexpressionEvalResult = false;
				} else {
					subexpressionEvalResult = true;
				}
			} else {
				// e-g: LHS="Happy", RHS="ppy"
				rstring str = Functions::String::substring(lhsValue,
					lhsLen-rhsLen, rhsLen);

				if(str == rhsValue) {
					subexpressionEvalResult = false;
				} else {
					subexpressionEvalResult = true;
				}
			}
		} else if(operationVerb == "in" || operationVerb == "inCI") {
			// In this case, RHS value is a list string literal.
			// e-g: [1, 2, 3, 4]
			// [1.4, 5.3, 98.65, 7.2]
			// ["Developer", "Tester", "Admin", "Manager"]
			//
			// We must convert this to a SPL list and then
			// do a membership test.
			try {
				// We can use spl_cast to convert a list string literal into
				// an SPL list. As long as the string representation truly
				// reflects an SPL list syntax, this conversion will work.
				// If not, it will throw an exception.
				// I learned about this technique by looking at the
				// auto generated C++ code for a similar conversion done in
				// a SPL application logic.
				const SPL::list<SPL::rstring> tokens =
					SPL::spl_cast<SPL::list<SPL::rstring>, SPL::rstring>::cast(rhsValue);

				if(operationVerb == "in") {
					// This is a case sensitive list membership test.
					subexpressionEvalResult =
						Functions::Collections::has(tokens, lhsValue);
				} else {
					// This is a case insensitive list membership test.
					// We will have to loop through every list element and
					// compare it in a case insensitive manner.
					subexpressionEvalResult = false;
					int32 listSize = Functions::Collections::size(tokens);
					rstring lhsValueLower = Functions::String::lower(lhsValue);

					for(int32 i=0; i<listSize; i++) {
						rstring rhsValueLower =
							Functions::String::lower(tokens[i]);

						if(Functions::String::findFirst(lhsValueLower, rhsValueLower) != -1) {
							// There is a list membership match.
							subexpressionEvalResult = true;
							break;
						}
					}
				}
			} catch(...) {
				// SPL type casting of a list string literal to SPL list failed.
				subexpressionEvalResult = false;
				error = INVALID_RHS_LIST_LITERAL_STRING_FOUND_FOR_IN_OR_IN_CI_OPVERB;
			}
		} else if(operationVerb == "containsCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);

			if(Functions::String::findFirst(lhsValueLower, rhsValueLower) != -1) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "notContainsCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);

			if(Functions::String::findFirst(lhsValueLower, rhsValueLower) == -1) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "startsWithCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);

			if(Functions::String::findFirst(lhsValueLower, rhsValueLower) == 0) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "notStartsWithCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);

			if(Functions::String::findFirst(lhsValueLower, rhsValueLower) != 0) {
				subexpressionEvalResult = true;
			} else {
				subexpressionEvalResult = false;
			}
		} else if(operationVerb == "endsWithCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);
			int32 lhsLen = Functions::String::length(lhsValueLower);
			int32 rhsLen = Functions::String::length(rhsValueLower);

			if(lhsLen < rhsLen) {
				subexpressionEvalResult = false;
			} else if(lhsLen == rhsLen) {
				if(lhsValueLower == rhsValueLower) {
					subexpressionEvalResult = true;
				} else {
					subexpressionEvalResult = false;
				}
			} else {
				// e-g: LHS="Happy", RHS="ppy"
				rstring str = Functions::String::substring(lhsValueLower,
					lhsLen-rhsLen, rhsLen);

				if(str == rhsValueLower) {
					subexpressionEvalResult = true;
				} else {
					subexpressionEvalResult = false;
				}
			}
		} else if(operationVerb == "notEndsWithCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);
			int32 lhsLen = Functions::String::length(lhsValueLower);
			int32 rhsLen = Functions::String::length(rhsValueLower);

			if(lhsLen < rhsLen) {
				subexpressionEvalResult = true;
			} else if(lhsLen == rhsLen) {
				if(lhsValueLower == rhsValueLower) {
					subexpressionEvalResult = false;
				} else {
					subexpressionEvalResult = true;
				}
			} else {
				// e-g: LHS="Happy", RHS="ppy"
				rstring str = Functions::String::substring(lhsValueLower,
					lhsLen-rhsLen, rhsLen);

				if(str == rhsValueLower) {
					subexpressionEvalResult = false;
				} else {
					subexpressionEvalResult = true;
				}
			}
		} else if(operationVerb == "equalsCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);
			subexpressionEvalResult = (lhsValueLower == rhsValueLower) ? true : false;
		} else if(operationVerb == "notEqualsCI") {
			rstring lhsValueLower = Functions::String::lower(lhsValue);
			rstring rhsValueLower = Functions::String::lower(rhsValue);
			subexpressionEvalResult = (lhsValueLower != rhsValueLower) ? true : false;
		} else if(operationVerb == "sizeEQ") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize == rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeNE") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize != rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeLT") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize < rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeLE") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize <= rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeGT") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize > rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeGE") {
			subexpressionEvalResult = false;
			int32 lhsSize = Functions::String::length(lhsValue);
			int32 rhsInt32 = atoi(rhsValue.c_str());

			if(lhsSize >= rhsInt32) {
				subexpressionEvalResult = true;
			}
		// For a string based relational operation, we will
		// convert it to an integer or float and then perform that task.
		// If it is not a number, we will then do a string lexical comparison.
		} else if(operationVerb == "<") {
			subexpressionEvalResult = false;

			if(isLhsValueString == true && isRhsValueString == true) {
				// They are pure strings. We will do a lexical comparison.
				int32 result = lhsValue.compare(rhsValue);

				if(result < 0) {
					subexpressionEvalResult = true;
				}
			} else if(isLhsValueFloat == false && isRhsValueFloat == false) {
				// The string values represent integers.
				lhsInt32 = atoi(lhsValue.c_str());
				rhsInt32 = atoi(rhsValue.c_str());

				if(lhsInt32 < rhsInt32) {
					subexpressionEvalResult = true;
				}
			} else {
				// The string values represent float.
				lhsFloat64 = atof(lhsValue.c_str());
				rhsFloat64 = atof(rhsValue.c_str());

				if(lhsFloat64 < rhsFloat64) {
					subexpressionEvalResult = true;
				}
			}
		} else if(operationVerb == "<=") {
			subexpressionEvalResult = false;

			if(isLhsValueString == true && isRhsValueString == true) {
				// They are pure strings. We will do a lexical comparison.
				int32 result = lhsValue.compare(rhsValue);

				if(result <= 0) {
					subexpressionEvalResult = true;
				}
			} else if(isLhsValueFloat == false && isRhsValueFloat == false) {
				// The string values represent integers.
				lhsInt32 = atoi(lhsValue.c_str());
				rhsInt32 = atoi(rhsValue.c_str());

				if(lhsInt32 <= rhsInt32) {
					subexpressionEvalResult = true;
				}
			} else {
				// The string values represent float.
				lhsFloat64 = atof(lhsValue.c_str());
				rhsFloat64 = atof(rhsValue.c_str());

				if(lhsFloat64 <= rhsFloat64) {
					subexpressionEvalResult = true;
				}
			}
		} else if(operationVerb == ">") {
			subexpressionEvalResult = false;

			if(isLhsValueString == true && isRhsValueString == true) {
				// They are pure strings. We will do a lexical comparison.
				int32 result = lhsValue.compare(rhsValue);

				if(result > 0) {
					subexpressionEvalResult = true;
				}
			} else if(isLhsValueFloat == false && isRhsValueFloat == false) {
				// The string values represent integers.
				lhsInt32 = atoi(lhsValue.c_str());
				rhsInt32 = atoi(rhsValue.c_str());

				if(lhsInt32 > rhsInt32) {
					subexpressionEvalResult = true;
				}
			} else {
				// The string values represent float.
				lhsFloat64 = atof(lhsValue.c_str());
				rhsFloat64 = atof(rhsValue.c_str());

				if(lhsFloat64 > rhsFloat64) {
					subexpressionEvalResult = true;
				}
			}
		} else if(operationVerb == ">=") {
			subexpressionEvalResult = false;

			if(isLhsValueString == true && isRhsValueString == true) {
				// They are pure strings. We will do a lexical comparison.
				int32 result = lhsValue.compare(rhsValue);

				if(result >= 0) {
					subexpressionEvalResult = true;
				}
			} else if(isLhsValueFloat == false && isRhsValueFloat == false) {
				// The string values represent integers.
				lhsInt32 = atoi(lhsValue.c_str());
				rhsInt32 = atoi(rhsValue.c_str());

				if(lhsInt32 >= rhsInt32) {
					subexpressionEvalResult = true;
				}
			} else {
				// The string values represent float.
				lhsFloat64 = atof(lhsValue.c_str());
				rhsFloat64 = atof(rhsValue.c_str());

				if(lhsFloat64 >= rhsFloat64) {
					subexpressionEvalResult = true;
				}
			}
		} else {
			// Unsupported operation verb for rstring.
			error = INVALID_RSTRING_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
			subexpressionEvalResult = false;
		}
    } // End of performRStringEvalOperations

    // This method checks if a given string is made of
    // numeric (integer or float) or alphanumeric (string) characters.
    inline boolean isNumber(rstring const & str) {
    	boolean periodCharacterFound = false;
    	int32 stringLength = Functions::String::length(str);
    	SPL::blob myBlob = Functions::String::convertToBlob(str);

    	// Stay in a loop and check if a given character is
    	// a numeric or alphabetical one.
    	for(int32 i=0; i<stringLength; i++) {
    		uint8 myChar = myBlob[i];

    		if (myChar >= 48 && myChar <= 57) {
    			// It falls within the range '0' to '9'.
    			// Keep checking the next characters.
    			continue;
    		} else {
    			if(myChar == 46 && periodCharacterFound == false) {
    				// ASCII 46 is a period character.
    				// In case of a float number, we will
    				// allow only one period character.
    				periodCharacterFound = true;
    				continue;
    			}

    			// This is not a numeric character.
    			// So, it is not a number.
    			return(false);
    		}
    	} // End of for loop.

    	if(stringLength == 1 && periodCharacterFound == true) {
    		// If the only character in this string is a period
    		// character, then it doesn't represent a number.
    		return(false);
    	}

    	// We have a number.
    	return(true);
    } // End of isNumber

    // This function performs the eval operations to check for the
    // existence of a given item in a set, list or map.
    inline void performCollectionItemExistenceEvalOperations(boolean const & itemExists,
    	rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error) {
    	error = ALL_CLEAR;

		// Allowed item existence check operations for a
    	// collection attribute type are these:
		// contains, notContains
		if(itemExists == true) {
			if(operationVerb == "contains") {
				subexpressionEvalResult = true;
			} else if(operationVerb == "notContains") {
				subexpressionEvalResult = false;
			} else {
				// Unsupported operation verb for a collection type.
				error = COLLECTION_ITEM_EXISTENCE_INVALID_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
				subexpressionEvalResult = false;
			}
		} else {
			if(operationVerb == "contains") {
				subexpressionEvalResult = false;
			} else if(operationVerb == "notContains") {
				subexpressionEvalResult = true;
			} else {
				// Unsupported operation verb for a collection type.
				error = COLLECTION_ITEM_EXISTENCE_INVALID_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
				subexpressionEvalResult = false;
			}
		}
    } // End of performCollectionItemExistenceEvalOperations

    // This function performs the eval operations to check for the
    // size of a given collection type i.e. set, list, map.
    inline void performCollectionSizeCheckEvalOperations(int32 const & lhsSize,
    	int32 const & rhsInt32, rstring const & operationVerb,
		boolean & subexpressionEvalResult, int32 & error) {
    	error = ALL_CLEAR;
    	subexpressionEvalResult = false;

		if(operationVerb == "sizeEQ") {
			if(lhsSize == rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeNE") {
			if(lhsSize != rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeLT") {
			if(lhsSize < rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeLE") {
			if(lhsSize <= rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeGT") {
			if(lhsSize > rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else if(operationVerb == "sizeGE") {
			if(lhsSize >= rhsInt32) {
				subexpressionEvalResult = true;
			}
		} else {
			// Unsupported operation verb for collection size check.
			error = INVALID_COLLECTION_SIZE_CHECK_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
		}
    } // End of performCollectionSizeCheckEvalOperations

    // This function performs the evaluations for non-string based
    // LHS and RHS values for relational and arithmetic operations.
    // For arithmetic evals, operationVerb will have extra stuff. e-g: % 8 ==
	template<class T1>
    inline void performRelationalOrArithmeticEvalOperations(T1 const & lhsValue,
    	T1 const & rhsValue, rstring const & operationVerb,
		T1 const & arithmeticOperandValue,
		rstring const & postArithmeticOperationVerb,
		boolean & subexpressionEvalResult, int32 & error) {
		error = ALL_CLEAR;

		if(operationVerb == "==") {
			subexpressionEvalResult = (lhsValue == rhsValue) ? true : false;
		} else if(operationVerb == "!=") {
			subexpressionEvalResult = (lhsValue != rhsValue) ? true : false;
		} else if(operationVerb == "<") {
			subexpressionEvalResult = (lhsValue < rhsValue) ? true : false;
		} else if(operationVerb == "<=") {
			subexpressionEvalResult = (lhsValue <= rhsValue) ? true : false;
		} else if(operationVerb == ">") {
			subexpressionEvalResult = (lhsValue > rhsValue) ? true : false;
		} else if(operationVerb == ">=") {
			subexpressionEvalResult = (lhsValue >= rhsValue) ? true : false;
		} else if(operationVerb == "+") {
			T1 result = lhsValue + arithmeticOperandValue;
			performPostArithmeticEvalOperations(result, rhsValue,
				postArithmeticOperationVerb, subexpressionEvalResult, error);
		} else if(operationVerb == "-") {
			T1 result = lhsValue - arithmeticOperandValue;
			performPostArithmeticEvalOperations(result, rhsValue,
				postArithmeticOperationVerb, subexpressionEvalResult, error);
		} else if(operationVerb == "*") {
			T1 result = lhsValue * arithmeticOperandValue;
			performPostArithmeticEvalOperations(result, rhsValue,
				postArithmeticOperationVerb, subexpressionEvalResult, error);
		} else if(operationVerb == "/") {
			if(arithmeticOperandValue == 0 || arithmeticOperandValue == 0.0) {
				error = DIVIDE_BY_ZERO_ARITHMETIC_FOUND_DURING_EXP_EVAL;
				subexpressionEvalResult = false;
			} else {
				T1 result = lhsValue / arithmeticOperandValue;
				performPostArithmeticEvalOperations(result, rhsValue,
					postArithmeticOperationVerb, subexpressionEvalResult, error);
			}
		} else if(operationVerb == "%") {
			// C++ doesn't allow modulus operation for float values.
			// So, with a generic template parameter T1 we have here,
			// it is not possible to do modulus only for non-float values.
			// We will get a compiler error since generic type T1 covers
			// both integer and non-integer types such as float32 and float64.
			// So, we are going to parcel the modulus work to overloaded
			// helper functions for performing the modulus calculation
			// separately for float32, float64 and the integer types.
			T1 result;
			calculateModulus(lhsValue, arithmeticOperandValue, result);
			performPostArithmeticEvalOperations(result, rhsValue,
				postArithmeticOperationVerb, subexpressionEvalResult, error);
		} else {
			// Unsupported operation verb for relational or arithmetic expression.
			error = RELATIONAL_OR_ARITHMETIC_INVALID_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
			subexpressionEvalResult = false;
		}
	} // End of performRelationalOrArithmeticEvalOperations

	// This is an overloaded modulus function for int32.
	inline void calculateModulus(int32 const & lhsValue,
		int32 const & arithmeticOperandValue, int32 & result) {
		result = lhsValue % arithmeticOperandValue;
	}

	// This is an overloaded modulus function for uint32.
	inline void calculateModulus(uint32 const & lhsValue,
		uint32 const & arithmeticOperandValue, uint32 & result) {
		result = lhsValue % arithmeticOperandValue;
	}

	// This is an overloaded modulus function for int64.
	inline void calculateModulus(int64 const & lhsValue,
		int64 const & arithmeticOperandValue, int64 & result) {
		result = lhsValue % arithmeticOperandValue;
	}

	// This is an overloaded modulus function for uint64.
	inline void calculateModulus(uint64 const & lhsValue,
		uint64 const & arithmeticOperandValue, uint64 & result) {
		result = lhsValue % arithmeticOperandValue;
	}

	// This is an overloaded modulus function for float32.
	inline void calculateModulus(float32 const & lhsValue,
		float32 const & arithmeticOperandValue, float32 & result) {
		// We will use the C++ fmod function instead of using the
		// % operator which is not supported for float values in C++.
		result = fmod(lhsValue, arithmeticOperandValue);
	}

	// This is an overloaded modulus function for float64.
	inline void calculateModulus(float64 const & lhsValue,
		float64 const & arithmeticOperandValue, float64 & result) {
		// We will use the C++ fmod function instead of using the
		// % operator which is not supported for float values in C++.
		result = fmod(lhsValue, arithmeticOperandValue);
	}

	// This is an overloaded modulus function for boolean.
	inline void calculateModulus(boolean const & lhsValue,
		boolean const & arithmeticOperandValue, boolean & result) {
		// This is a dummy one for boolean type and it will never get called.
		result = false;
	}

	// This function performs the evaluation of a given arithmetic result with
	// a given RHS value using the specified post arithmetic operation verb.
	template<class T1>
	inline void performPostArithmeticEvalOperations(T1 const & arithmeticResult,
		T1 const & rhsValue, rstring const & postArithmeticOperationVerb,
		boolean & subexpressionEvalResult, int32 & error) {
		error = ALL_CLEAR;

		if(postArithmeticOperationVerb == "==") {
			subexpressionEvalResult = (arithmeticResult == rhsValue) ? true : false;
		} else if(postArithmeticOperationVerb == "!=") {
			subexpressionEvalResult = (arithmeticResult != rhsValue) ? true : false;
		} else if(postArithmeticOperationVerb == "<") {
			subexpressionEvalResult = (arithmeticResult < rhsValue) ? true : false;
		} else if(postArithmeticOperationVerb == "<=") {
			subexpressionEvalResult = (arithmeticResult <= rhsValue) ? true : false;
		} else if(postArithmeticOperationVerb == ">") {
			subexpressionEvalResult = (arithmeticResult > rhsValue) ? true : false;
		} else if(postArithmeticOperationVerb == ">=") {
			subexpressionEvalResult = (arithmeticResult >= rhsValue) ? true : false;
		} else {
			// Unsupported post arithmetic operation verb.
			error = INVALID_POST_ARITHMETIC_OPERATION_VERB_FOUND_DURING_EXP_EVAL;
			subexpressionEvalResult = false;
		}
	} // End of performPostArithmeticEvalOperations

	// This function creates the next subexpression id which is
	// needed in the expression validation method.
	// This function is always called after a subexpression got
	// processed successfully. Depending on which stage the
	// expression processing takes place, this function can get
	// called from different sections in the expression validation method.
	inline void getNextSubexpressionId(int32 const & currentNestedSubexpressionLevel,
		rstring & subexpressionId) {
		// Depending on the nested or non-nested nature,
		// subexpression id will carry a different
		// number value in a string variable.
    	// e-g: "1.1", "2.1", "2.2", "2.3", "3.1", "4.1", "4.2", "4.3",
		//      "4.4", "5.1", "5.2", "5.3", "6.1", "6.2", "7.1", "7.2"
		//
		// A few examples of zero, single and nested parenthesis.
		//
		// 1.1             2.1                 3.1           4.1
		// a == "hi" && b contains "xyz" && g[4] > 6.7 && id % 8 == 3
		//
		// 1.1                               2.1
		// (a == "hi") && (b contains "xyz" || g[4] > 6.7 || id % 8 == 3)
		//
		// 1.1                2.1                   3.1             4.1
		// (a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)
		//
		// 1.1                          2.1                       2.2
		// (a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)
		//
		// 1.1                 2.1                       2.2
		// (a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))
		//
		// 1.1                                 2.1
		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))
		//
		// 1.1                     2.1                 2.2
		// (a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))
		//
		// 1.1                                        1.2                           2.1
		// ((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)
		//
		// 1.1                                                                   1.2                          1.3
		// (((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')
		//
		if(subexpressionId == "") {
			// This is the very first subexpression that got
			// processed within this expression. So, we only
			// need to initialize it to the starting subexpression id.
			subexpressionId = "1.1";
			return;
		}

		SPL::list<rstring> tokens =
			Functions::String::tokenize(subexpressionId, ".", false);
		subexpressionId = "";

		// We are going to take care of the following three conditions in the
		// first half of the if block below.
		// 1) If we have an expression with zero open and close parenthesis,
		// then we will have the subexpression id as 1.1, 2.1, 3.1 and so on.
		// It is a non-nested subexpression with current nested level set to 0.
		// Refer to the first example expression above.
		//
		// 2) If we have an expression with single level non-nested
		// open and close parenthesis, then we will have the
		// subexpression id as 1.1, 2.1, 3.1 and so on.
		// In the non-nested case, the number of open and close
		// parenthesis will be the same after a subexpression is processed.
		// It is a non-nested subexpression with current nested level set to 0.
		// Refer to the second and third example expressions above.
		//
		// 3) If we are here because of the open and close parenthesis not
		// being the same, that means this is the nested case.
		// If the current nested subexpression level is 1, that means
		// this is a fresh beginning of a nested subexpression.
		// In that case, we will increment at level 1 and then set
		// the level 2 to 1.
		// Refer to example expressions 4 to 9 above.
		//
		if(currentNestedSubexpressionLevel <= 1){
			// For these three conditions, it is
			// sufficient to increment only at level 1.
			int32 value = atoi(tokens[0].c_str());
			value++;
			// Store it back as a string.
			ostringstream valueString;
			valueString << value;
			subexpressionId = valueString.str();
			// In this case, we will always set the level 2 value to "1".
			subexpressionId += ".1";
		} else {
			// This is a nested subexpression that is after its
			// fresh beginning. So, we will increment only at level 2.
			// Let us keep the existing value at level 1.
			subexpressionId = tokens[0];
			int32 value = atoi(tokens[1].c_str());
			value++;
			// Store it back as a string.
			ostringstream valueString;
			valueString << value;
			subexpressionId += "." + valueString.str();
		}
	} // End of getNextSubexpressionId

	// This function checks if the next non-space character is
	// an open parenthesis.
	inline boolean isNextNonSpaceCharacterOpenParenthesis(blob const & myBlob,
		int32 const & idx, int32 const & stringLength) {
		for(int32 i=idx; i<stringLength-1; i++) {
			if(myBlob[i+1] == ' ') {
				// Skip all space characters that are there before
				// we can see a non-space character.
				continue;
			}

			if(myBlob[i+1] == '(') {
				// Next non-space character is an open parenthesis.
				return(true);
			} else {
				return(false);
			}
		}

		return(false);
	} // End of isNextNonSpaceCharacterOpenParenthesis

	// This function is called from the expression validation
	// function when processing an open parenthesis.
	// It checks if the current single subexpression is
	// enclosed within a parenthesis.
	// e-g: (b contains "xyz")
	inline boolean isThisAnEnclosedSingleSubexpression(rstring const & expr,
		int32 const & idx) {
		// We have to find if a close parenthesis comes before or
		// after a possible logical operator. If it comes before,
		// then this is an enclosed single subexpression.
		int32 idx1 = Functions::String::findFirst(expr, ")", idx);

		if(idx1 == -1) {
			// Close parenthesis not found.
			return(false);
		}

		int32 idx2 = -1;
		// Find a logical OR.
		idx2 = Functions::String::findFirst(expr, "||", idx);

		if(idx2 == -1) {
			// There is no logical OR. See if we can find a logical AND.
			idx2 = Functions::String::findFirst(expr, "&&", idx);

			if(idx2 == -1) {
				// There is no logical AND.
				// That means, this is an enclosed single subexpression.
				return(true);
			}
		}

		// If a close parenthesis appears before a logical operator,
		// then this is an enclosed single subexpression.
		if(idx1 < idx2) {
			return(true);
		} else {
			return(false);
		}
	} // End of isThisAnEnclosedSingleSubexpression.

	// This function checks if the next non-space character is
	// a close parenthesis.
	inline boolean isNextNonSpaceCharacterCloseParenthesis(blob const & myBlob,
		int32 const & idx, int32 const & stringLength) {
		for(int32 i=idx; i<stringLength-1; i++) {
			if(myBlob[i+1] == ' ') {
				// Skip all space characters that are there before
				// we can see a non-space character.
				continue;
			}

			if(myBlob[i+1] == ')') {
				// Next non-space character is a close parenthesis.
				return(true);
			} else {
				return(false);
			}
		}

		return(false);
	} // End of isNextNonSpaceCharacterCloseParenthesis

	// This function checks if the given subexpression id is
	// in a nested group. If it is, then it gets the
	// relevant details such as the number of subexpressions
	// in that group and the intra nested subexpression logical operator.
	inline void getNestedSubexpressionGroupInfo(rstring const & subexpressionId,
		SPL::list<rstring> const & subexpressionIdsList,
		SPL::map<rstring, rstring> const & intraNestedSubexpressionLogicalOperatorsMap,
		int32 & subexpressionCntInCurrentNestedGroup,
		rstring & intraNestedSubexpressionLogicalOperator) {
		// This function is called from the evaluateExpression method.
		// Since all the subexpression ids in a nested group will
		// appear in a sequential (sorted) order, this function can be called
		// only once per nested group to get the important details needed for
		// evaluating them together.
		subexpressionCntInCurrentNestedGroup = 0;
		intraNestedSubexpressionLogicalOperator = "";

		// Let us check if the given subexpression id is in a nested group.
		// e-g: 2.1, 2.2, 2.3  They all carry the same level 1.
		SPL::list<rstring> tokens1 =
			Functions::String::tokenize(subexpressionId, ".", false);
		// We have to compare only at level 1 of the subexpression id.
		// e-g: 2.4  Here, level 1 is 2 and level 2 is 4.
		int32 myId = atoi(tokens1[0].c_str());
		int32 subexpressionIdsListSize =
			Functions::Collections::size(subexpressionIdsList);

		for(int32 i=0; i<subexpressionIdsListSize; i++) {
			rstring idString = subexpressionIdsList[i];
			SPL::list<rstring> tokens2 =
				Functions::String::tokenize(idString, ".", false);
			// We have to compare only at level 1 of the subexpression id.
			// e-g: 2.4  Here, level 1 is 2 and level 2 is 4.
			int32 currentId = atoi(tokens2[0].c_str());

			if(currentId == myId) {
				// We have a match. This could be part of the same nested group.
				subexpressionCntInCurrentNestedGroup++;
			}
		} // End of for loop.

		// If we only find more than one entry with the
		// same level 1, then that is considered to be in
		// a nested group.
		if(subexpressionCntInCurrentNestedGroup > 1) {
			// It is sufficient to get only one of this logical operator.
			// Because at the time of validation, we already verified that
			// they are all of the same kind within a given nested group.
			intraNestedSubexpressionLogicalOperator =
				intraNestedSubexpressionLogicalOperatorsMap.at(subexpressionId);
		} else {
			// This one is not in a nested group. So set it to 0.
			subexpressionCntInCurrentNestedGroup = 0;
		}
	} // End of getNestedSubexpressionGroupInfo

	// This method checks to see if a quote character is
	// embedded within a map key or if it represents an
	// end of a map key string. This method gets
	// called from the validateExpression and
	// validateTupleAttribute methods.
	inline boolean isQuoteCharacterAtEndOfMapKeyString(blob const & myBlob, int32 const & idx) {
		// We will allow single or double quote characters within a given
		// map key. This method checks whether a given quote character appears
		// before it is immediately followed by a ] character.
		int32 blobSize = Functions::Collections::size(myBlob);

		if(idx >= blobSize) {
			// Given idx falls outside the blob.
			return(false);
		}

		if(myBlob[idx] != '"' && myBlob[idx] != '\'') {
			// Given idx position doesn't hold a quote character.
			return(false);
		}

		// Let us check if the next non-space character that we see is a ] character.
		for(int32 i=idx+1; i<blobSize; i++) {
			// If there are space characters appearing
			// right after the given quote character, skip them.
			if(myBlob[i] == ' ') {
				continue;
			}

			if(myBlob[i] == ']') {
				// The quote character is immediately followed by a ] character.
				// In that case, it marks the end of the map key string.
				// * * * * L O G I C  A L E R T * * * *
				// This logic assumes that the map key string itself won't
				// have a ] character immediately after an embedded quote
				// character as part of the key. This is a known assumption and
				// a restriction in this logic at this time. If this assumption is
				// not correct and if it produces a wrong result in real-life
				// applications, then we have to adjust the logic in this method.
				// Example of such a condition: Body.MachineStatus.Status["K(\']e[y)2"]
				// As it can be seen, an embedded quote character is immediately
				// followed by a ] character. That rare condition will produce a
				// wrong result in this logic.
				return(true);
			} else {
				// We see a character other than ].
				// That means, the given quote character is not at the very end of the map key.
				return(false);
			}
		} // End of for loop.

		// We reached the end of the expression string.
		// That means, we didn't see a ] character.
		// Something is wrong with the expression string.
		return(false);
	} // End of isQuoteCharacterAtEndOfMapKeyString

	// This method checks to see if a quote character is
	// embedded within a RHS string or if it represents an
	// end of an RHS string. This method gets
	// called from the validateExpression method.
	inline boolean isQuoteCharacterAtEndOfRhsString(blob const & myBlob, int32 const & idx) {
		// We will allow single or double quote characters within a given
		// RHS string. This method checks to see whether a given
		// quote character appears before it is followed
		// either by an end of the given expression string or by
		// one of the logical operators.
		int32 blobSize = Functions::Collections::size(myBlob);

		if(idx >= blobSize) {
			// Given idx falls outside the blob.
			return(false);
		}

		if(myBlob[idx] != '"' && myBlob[idx] != '\'') {
			// Given idx position doesn't hold a quote character.
			return(false);
		}

		// Let us check if the next non-space character leads us to
		// a logical operator i.e. && or ||.
		for(int32 i=idx+1; i<blobSize; i++) {
			// If we encounter another quote character, then the
			// current quote character from where this method is
			// being called is not at the end of the given RHS string.
			if(myBlob[i] == '"' || myBlob[i] == '\'') {
				return(false);
			}

			// Let us now check for the character sequence "&& ".
			if(myBlob[i] == '&') {
				// The quote character is followed by a & character.
				// In that case, let us check if the next character in the
				// sequence is another & followed by a space.
				if(i < blobSize-2 && myBlob[i+1] == '&' &&
					myBlob[i+2] == ' ') {
					// This must be an end of quote character in
					// the given RHS string.
					return(true);
				}
			}

			// Let us now check for the character sequence "|| ".
			if(myBlob[i] == '|') {
				// The quote character is followed by a | character.
				// In that case, let us check if the next character in the
				// sequence is another | followed by a space.
				if(i < blobSize-2 && myBlob[i+1] == '|' &&
					myBlob[i+2] == ' ') {
					// This must be an end of quote character in
					// the given RHS string.
					return(true);
				}
			}

			// Continue the loop.
		} // End of for loop.

		// We reached the end of the expression string.
		// That means, the given quote character is the end of the RHS string.
		return(true);
	} // End of isQuoteCharacterAtEndOfRhsString

	// This method checks to see if a close bracket ] character is
	// embedded within a RHS list literal string or if it represents an
	// end of an RHS list literal string. This method gets
	// called from the validateExpression method.
	inline boolean isCloseBracketAtEndOfRhsString(blob const & myBlob, int32 const & idx) {
		// We will allow close bracket characters within a given
		// RHS string. This method checks to see whether a given
		// close bracket character appears before it is followed
		// either by an end of the given expression string or by
		// one of the logical operators.
		int32 blobSize = Functions::Collections::size(myBlob);

		if(idx >= blobSize) {
			// Given idx falls outside the blob.
			return(false);
		}

		if(myBlob[idx] != ']') {
			// Given idx position doesn't hold a close bracket character.
			return(false);
		}

		// Let us check if the next non-space character leads us to
		// a logical operator i.e. && or ||.
		for(int32 i=idx+1; i<blobSize; i++) {
			// If we encounter another close bracket character, then the
			// current ] character from where this method is
			// being called is not at the end of the given RHS string.
			if(myBlob[i] == ']') {
				return(false);
			}

			// Let us now check for the character sequence "&& ".
			if(myBlob[i] == '&') {
				// The close bracket character is followed by a & character.
				// In that case, let us check if the next character in the
				// sequence is another & followed by a space.
				if(i < blobSize-2 && myBlob[i+1] == '&' &&
					myBlob[i+2] == ' ') {
					// This must be a close bracket character in
					// the given RHS list string literal.
					return(true);
				}
			}

			// Let us now check for the character sequence "|| ".
			if(myBlob[i] == '|') {
				// The close bracket character is followed by a | character.
				// In that case, let us check if the next character in the
				// sequence is another | followed by a space.
				if(i < blobSize-2 && myBlob[i+1] == '|' &&
					myBlob[i+2] == ' ') {
					// This must be a close bracket character in
					// the given RHS list string literal.
					return(true);
				}
			}

			// Continue the loop.
		} // End of for loop.

		// We reached the end of the expression string.
		// That means, the given close bracket character is the end of the RHS string.
		return(true);
	} // End of isCloseBracketAtEndOfRhsString

	// This function fetches the value of a user given
	// attribute present in a user given tuple. This function can be
	// directly called from an SPL application code.
	//
    // Example tuple attribute names:
    // "symbol"
	// "employee.skills"
	// "stats.numberOfSchools"
	// "details.location.geo.latitude"
	// "details.location.info.officials"
    //
	// Get the value of a given tuple attribute name.
	// Arg1: Fully qualified attribute name
	// Arg2: Your tuple
	// Arg3: A mutable variable of an appropriate type in which the
	//       value of a given attribute will be returned.
	// Arg4: A mutable int32 variable to receive non-zero error code if any.
	// Arg5: A boolean value to enable debug tracing inside this function.
	// It is a void method that returns nothing.
	//
	template<class T1, class T2>
	inline void get_tuple_attribute_value(rstring const & attributeName,
		T1 const & myTuple, T2 & value, int32 & error, boolean const & trace) {
	    boolean result = false;
    	error = ALL_CLEAR;

    	// Check if there is some content in the given attribute name.
    	if(Functions::String::length(attributeName) == 0) {
    		error = EMPTY_ATTRIBUTE_NAME_GIVEN_FOR_VALUE_FETCHING;
    		return;
    	}

    	// Get the string literal of a give tuple.
		// Example of myTuple's schema:
		// myTuple=tuple<rstring name,tuple<tuple<tuple<float32 latitude,float32 longitude> geo,tuple<rstring state,rstring zipCode,map<rstring,rstring> officials,list<rstring> businesses> info> location,tuple<float32 temperature,float32 humidity> weather> details,tuple<int32 population,int32 numberOfSchools,int32 numberOfHospitals> stats,int32 rank,list<int32> roadwayNumbers,map<rstring,int32> housingNumbers>
		//
    	rstring myTupleSchema = getSPLTypeName(myTuple, trace);

    	if(myTupleSchema == "") {
    		// This should never occur. If it happens in
    		// extremely rare cases, we have to investigate the
    		// tuple literal schema generation function.
    		error = TUPLE_LITERAL_SCHEMA_GENERATION_ERROR;
    		return;
    	}

		// Let us parse the individual attributes of the given tuple and
    	// store them in a map.
		SPL::map<rstring, rstring> tupleAttributesMap;
		result = parseTupleAttributes(myTupleSchema,
			tupleAttributesMap, error, trace);

		if(result == false) {
			return;
		}

		// Let us now validate the user given attribute name for
		// its syntax correctness.
		SPL::list<rstring> attributeNameLayoutList;
		// We are making a non-recursive call. So, start from
		// the very first index i.e. index 0 of the given attribute name.
		int32 validationStartIdx = 0;
		result = validateTupleAttributeName(attributeName,
			tupleAttributesMap, attributeNameLayoutList,
		    error, validationStartIdx, trace);

		if(result == false) {
			return;
		}

		// We can now do the final step in getting the tuple attribute value.
		// Let us now look deep inside of this tuple and fetch the
		// value of a given attribute.
	    fetchTupleAttributeValue(attributeName, tupleAttributesMap,
	    	attributeNameLayoutList, myTuple, value, error, trace);
    } // End of get_tuple_attribute_value

	// This method validates the user given tuple attribute name for
	// its syntax correctness. It is called from the
	// get_tuple_attribute_value method above.
    inline boolean validateTupleAttributeName(rstring const & attributeName,
        SPL::map<rstring, rstring> const & tupleAttributesMap,
    	SPL::list<rstring> & attributeNameLayoutList,
    	int32 & error, int32 & validationStartIdx, boolean trace=false) {
    	error = ALL_CLEAR;

    	// This method will do its work and keep populating the
    	// attribute name layout list which is passed as an
    	// input reference argument.
    	//
    	// This method will get called recursively when a list<TUPLE> is
    	// encountered in a given attribute name. In that case, the
    	// validationStartIdx method argument will tell us where to
    	// start the validation from. In a non-recursive call into
    	// this method, validationStartIdx is set to 0 to start the
    	// validation from the beginning of the attribute name.
    	// It is important to note that the recursive caller must always pass
    	// its own newly formed local variables as method arguments here so as
    	// not to overwrite the original reference pointers passed by the
    	// very first caller who made the initial non-recursive call.
    	//
    	// When fetching the value of an attribute name involving a list<TUPLE>,
    	// our fetchTupleAttributeValue method available later in this file will
    	// also call this validation method in a non-recursive manner. Since it
    	// has to get value of only that single TUPLE attribute, it will only
    	// send that single TUPLE attribute string on its own starting at index 0.
    	// That method will set the validationStartIdx to 0. That is perfectly
    	// fine for doing a one shot single TUPLE attribute validation.
    	// The main purpose there is to build the attribute name layout list for
    	// that TUPLE attribute involving a list<TUPLE> as that is a
    	// key component in getting the attribute value.
    	//
    	// So, there is nothing to be concerned about the way this method will get
    	// called in a few special ways as explained above in those two paragraphs.
    	//
    	// Get a blob representation of the attribute name.
    	SPL::blob myBlob = Functions::String::convertToBlob(attributeName);
    	int32 stringLength = Functions::String::length(attributeName);
    	uint8 currentChar = 0, previousChar = 0;

    	// At this time, we can validate the given attribute name.
    	int32 idx = 0;
    	boolean lhsFound = false;
    	boolean lhsSubscriptForListAndMapAdded = false;
    	//
    	// Please note that this function receives a reference to a
    	// list as an argument where it will keep storing all the
    	// tuple attribute related details. That list is an important one which
    	// acts as a structure describing the composition of the
    	// given tuple attribute. Its structure is as explained here.
		//
    	// This list describes the composition of a tuple attribute.
    	// Structure of such a list will go something like this:
    	// This list will have a sequence of rstring items as shown below.
    	// LHSAttribName
    	// LHSAttribType
    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	// Optional entry for list<TUPLE> attribute name start index - When N/A, this entry will not be there.
    	// Optional entry for list<TUPLE> attribute name end index - When N/A, this entry will not be there.
    	//
    	// If this method is being called recursively, we can
    	// directly go to validate the attribute name to which
    	// the caller must have set the validation start index.
    	if(validationStartIdx > 0) {
    		idx = validationStartIdx;
    	}

    	// Stay in this loop and validate the tuple attribute name and
    	// keep building the attribute name layout list structure which will be
    	// useful in the next major step in another function.
    	while(idx < stringLength) {
    		if(idx > 0) {
    			previousChar = myBlob[idx-1];
    		}

       		currentChar = myBlob[idx];

    		// If it is a space character, move to the next position.
    		if(currentChar == ' ') {
    			idx++;
    			continue;
    		}

    		// If we have not yet found the lhs i.e. the tuple attribute name,
    		// let us try to match with what is allowed based on the user given tuple.
    		// e-g:
    		// weatherList[0].sunnyDay
    		// stats.numberOfSchools
    		// roadwayNumbers[5]
    		// details.location.geo.latitude
    		// details.location.info.officials['abc']
    		if(lhsFound == false) {
    			rstring lhsAttribName = "";
    			rstring lhsAttribType = "";
    			lhsSubscriptForListAndMapAdded = false;
				ConstMapIterator it = tupleAttributesMap.getBeginIterator();

				while (it != tupleAttributesMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<rstring,rstring> const & myRStringRString = myVal;
					lhsAttribName = myRStringRString.first;
					lhsAttribType = myRStringRString.second;

					int idx2 = Functions::String::findFirst(attributeName, lhsAttribName, idx);

					if(idx2 == idx) {
						// We must ensure that it is an exact match.
						// That means at the end of this attribute name,
						// we can only have either a space or a [ character.
						int32 lengthOfAttributeName =
							Functions::String::length(lhsAttribName);

						if((idx + lengthOfAttributeName) < stringLength &&
							myBlob[idx + lengthOfAttributeName] != ' ' &&
							myBlob[idx + lengthOfAttributeName] != '[') {
							// This means, we only had a partial attribute name match.
							// Let us continue the loop and look for finding an
							// exact match with some other attribute name in the given tuple.
							//
							// This line is here to debug when an LHS gets
							// matched partially with a tuple attribute name.
							if(trace == true) {
								cout << "_TTTTT_ ^" <<
									myBlob[idx + lengthOfAttributeName] <<
									"^ AttribName=" << lhsAttribName << endl;
							}

							// Increment the map iterator before continuing the loop.
							it++;
							continue;
						}

						// We have a good start in finding an attribute.
						// Add the lhs tuple attribute name and
						// its type to the attribute name layout list.
				    	// This list will have a sequence of rstring items as shown below.
				    	// LHSAttribName
				    	// LHSAttribType
				    	// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
				    	// Optional entry for list<TUPLE> attribute name start index - When N/A, this entry will not be there.
				    	// Optional entry for list<TUPLE> attribute name end index - When N/A, this entry will not be there.
				    	//
						// e-g:
			    		// weatherList[0].sunnyDay
			    		// stats.numberOfSchools
			    		// roadwayNumbers[5]
			    		// details.location.geo.latitude
			    		// details.location.info.officials['abc']
						Functions::Collections::appendM(attributeNameLayoutList,
							lhsAttribName);
						Functions::Collections::appendM(attributeNameLayoutList,
							lhsAttribType);
						// If it is list or map, we must ensure that the next
						// character sequence is [3] for list and
						// ["xyz"] or [123] or [12.34]  for map.
						// Move the idx past the end of the matching attribute name.
						idx += Functions::String::length(lhsAttribName);

						// If we have a list, it can be for list of
						// SPL built-in data types or for list<TUPLE>.
						// We will process both of them here.
						// If it is a list<TUPLE>, then we will do additional
						// validation of the attribute access inside of
						// that tuple in a different code block outside of
						// this if block.
						if(Functions::String::findFirst(lhsAttribType, "list") == 0) {
							// We must ensure that it has [n] following it.
							// The next character we find must be open square bracket [.
							boolean openSquareBracketFound = false;

							while(idx < stringLength) {
								// Skip any space characters preceding the [ character.
								if(myBlob[idx] == ' ') {
									idx++;
									continue;
								} else if(myBlob[idx] == '[') {
									openSquareBracketFound = true;
									break;
								} else {
									// A non-space or non-[ character found which is not valid.
									if(Functions::String::findFirst(lhsAttribType, "list<tuple<" ) == 0) {
										error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST_OF_TUPLE;
									} else {
										error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST;
									}

									return(false);
								}

								idx++;
							} // End of inner while loop.

							if(openSquareBracketFound == false) {
								// This is allowed if the user wants to
								// get the value of the entire list instead of
								// just a single element from the list.
								// Insert an empty string in the attribute name
								// layout list to indicate that we don't have
								// a list or map index.
								rstring str = "";
								Functions::Collections::appendM(attributeNameLayoutList, str);
								// We completed the validation of the lhs.
								lhsFound = true;
								// We can break out of the inner while loop that
								// iterates over the tuple attributes map so that
								// we can finish validating the attribute name.
								break;
							} // End of if(openSquareBracketFound == false)

							// After the open square bracket, we can have a
							// number as the list index.
							// Move past the open square bracket.
							idx++;
							boolean allNumeralsFound = false;
							boolean closeSquareBracketFound = false;
							boolean spaceFoundAfterListIndexValue = false;
							rstring listIndexValue =  "";

							// Let us ensure that we see all numerals until we see a
							// close square bracket ].
							while(idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterListIndexValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the list index value.
									if(listIndexValue != "") {
										spaceFoundAfterListIndexValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// A non-numeral found.
									allNumeralsFound = false;
									break;
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterListIndexValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									listIndexValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop.

							if(spaceFoundAfterListIndexValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_LIST_INDEX;
								return(false);
							}

							if(allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_AS_LIST_INDEX;
								return(false);
							}

							if(closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_AFTER_LIST;
								return(false);
							}

							// This list attribute is now fully validated.
							// Move past the ] character.
							idx++;
							// We can insert the list index into our attribute name layout list.
							Functions::Collections::appendM(attributeNameLayoutList,
								listIndexValue);
							lhsSubscriptForListAndMapAdded = true;
						} // End of if block checking list attribute's [n]

						// If it is a list<TUPLE>, let us now do additional
						// validation for the attribute access within the
						// tuple held inside a given list.
						// e-g: Body.MachineStatus.ComponentList[0].Component["MapKey"] == "MapValue"
						if(Functions::String::findFirst(lhsAttribType, "list<tuple<") == 0) {
							// This must be a . i.e. period character to indicate that a
							// tuple attribute access is being made.
							if(idx < stringLength && myBlob[idx] != '.') {
								// No period found for tuple attribute access.
								error = NO_PERIOD_FOUND_AFTER_LIST_OF_TUPLE;
								return(false);
							}

							// Move past the period character.
							idx++;

							// Use of "lot" in the local variable names below means List Of Tuple.
		    				// We can now get the attributes of the tuple held
							// inside a list<TUPLE>.
		    				//
		    				int32 lotSchemaLength =
		    					Functions::String::length(lhsAttribType);

		    				// Get the tuple<...> part from this attribute's type.
		    				// We have to skip the initial "list<" portion in the type string.
		    				// We also have to skip the final > in the type string that
		    				// is a closure for "list<" i.e. we start from the
		    				// zero based index 5 and take the substring until the
		    				// end except for the final > which in total is 6 characters
		    				// less than the entire type string.
		    				rstring lotTupleSchema =
		    					Functions::String::substring(lhsAttribType,
								5, lotSchemaLength-6);
		    				SPL::map<rstring, rstring> lotTupleAttributesMap;
		    				int32 lotError = 0;
		    				boolean lotResult = parseTupleAttributes(lotTupleSchema,
		    					lotTupleAttributesMap, lotError, trace);

		    				if(lotResult == false) {
		    					// This error should never happen since we
		    					// already passed this check even before
		    					// coming into this validation method.
		    					// We are simply doing it here as a safety measure.
		    					error = ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_VALIDATION;

		    					if(trace == true) {
			    					cout << "It failed to get the list<TUPLE> attributes for " <<
			    						lhsAttribName <<
										" during attribute name validation. Error=" << lotError <<
										". Tuple schema=" << lotTupleSchema << endl;
		    					}

		    					return(false);
		    				}

							// We got the LOT tuple attributes.
							// We can recursively call the current
							// method that we are in now to validate the
							// tuple attribute access involving a list<TUPLE>.
							if(trace == true) {
								cout << "BEGIN Recursive validate attribute name call for list<TUPLE> " <<
									lhsAttribName << "." << endl;
							}

							SPL::list<rstring> lotAttributeNameLayoutList;
							// It is a recursive call. So, we can tell this method to
							// start the validation at a specific position in the
							// attribute name instead of from index 0. It can be done by
							// setting the following variable to a non-zero value.
							validationStartIdx = idx;
							// We will also store the index where the
							// attribute name involving a list<TUPLE> starts for another
							// use at the end of this additional processing we are
							// doing here for a list<TUPLE>.
							int32 lotAttributeNameStartIdx = idx;
							lotResult = validateTupleAttributeName(attributeName, lotTupleAttributesMap,
								lotAttributeNameLayoutList, error,
								validationStartIdx, trace);

							if(trace == true) {
								cout << "END Recursive validate attribute name call for list<TUPLE> " <<
									lhsAttribName << "." << endl;
							}

							if(lotResult == false) {
								// There was a validation error in the recursive call.
								return(false);
							}

							// Now that the recursive call is over, we can set idx to a
							// position up to which the recursive call did the validation.
							idx = validationStartIdx;
							// We must reset it now.
							validationStartIdx = 0;
							// At this point, we correctly validated the
							// attribute name involving a list<TUPLE>.
							// In the recursive call we just returned back from,
							// it has already successfully completed the
							// validation for LHS attribute name.
							// So, we can set it as cleanly completed.
			    			lhsFound = true;
			    			//
			    			// LHSAttribName
			    			// LHSAttribType
			    			// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
			    	    	// Optional entry for list<TUPLE> attribute name start index - When N/A, this entry will not be there.
			    	    	// Optional entry for list<TUPLE> attribute name end index - When N/A, this entry will not be there.
			    			//
			    			// Just because this is a list<TUPLE>, we already
			    			// appended a value for the list index entry of
			    			// the attribute name layout list in the previous
			    			// if code block for close bracket detection.
			    			// Now, we can fill the lotAttributeNameStartIdx in
			    			// the attribute name layout list as the next item.
			    			// We will also store the current idx position where
			    			// LOT attribute name ends as the last item in that list.
			    			// These two values will be used later inside
			    			// the fetchTupleAttributeValue method. This is so important for
			    			// fetching an attribute value involving a list<TUPLE>.
					    	ostringstream lotAttribStartIdx;
					    	lotAttribStartIdx << lotAttributeNameStartIdx;
					    	rstring myValueString = lotAttribStartIdx.str();
							Functions::Collections::appendM(attributeNameLayoutList, myValueString);
					    	// RHS entry.
					    	ostringstream lotAttribEndIdx;
					    	lotAttribEndIdx << idx;
					    	myValueString = lotAttribEndIdx.str();
							Functions::Collections::appendM(attributeNameLayoutList, myValueString);
						} // End of the additional validation for list<TUPLE>

						if(Functions::String::findFirst(lhsAttribType, "map") == 0) {
							// It is a map. We must ensure that it has a valid access scheme following it.
							// We support only very specific things here.
							// e-g: [3], ["xyz"], [45.23]
							// The next character we find must be open square bracket [.
							boolean openSquareBracketFound = false;

							while(idx < stringLength) {
								// Skip any space characters preceding the [ character.
								if(myBlob[idx] == ' ') {
									idx++;
									continue;
								} else if(myBlob[idx] == '[') {
									openSquareBracketFound = true;
									break;
								} else {
									// A non-space or non-[ character found which is not valid.
									error = OPEN_SQUARE_BRACKET_NOT_FOUND_AFTER_MAP;
									return(false);
								}

								idx++;
							} // End of inner while loop.

							if(openSquareBracketFound == false) {
								// This is allowed if the user wants to
								// get the value of the entire map instead of
								// just a single element from the map.
								// Insert an empty string in the attribute name
								// layout list to indicate that we don't have
								// a list or map index.
								rstring str = "";
								Functions::Collections::appendM(attributeNameLayoutList, str);
								// We completed the validation of the lhs.
								lhsFound = true;
								// We can break out of the inner while loop that
								// iterates over the tuple attributes map so that
								// we can finish validating the attribute name.
								break;
							} // End of if(openSquareBracketFound == false)

							// After the open square bracket, we can have an
							// integer or a float or a string as the map key.
							boolean intMapKey = false;
							boolean floatMapKey = false;
							boolean stringMapKey = false;

							if(Functions::String::findFirst(lhsAttribType, "map<int") == 0) {
								intMapKey = true;
							} else if(Functions::String::findFirst(lhsAttribType, "map<float") == 0) {
								floatMapKey = true;
							} else if(Functions::String::findFirst(lhsAttribType, "map<rstring") == 0) {
								stringMapKey = true;
							} else {
								error = UNSUPPORTED_KEY_TYPE_FOUND_IN_MAP;
								return(false);
							}

							// Move past the open square bracket.
							idx++;
							boolean allNumeralsFound = false;
							int decimalPointCnt = 0;
							boolean openQuoteFound = false;
							boolean closeQuoteFound = false;
							boolean invalidStringCharacterFound = false;
							boolean stringCharacterFoundAfterCloseQuote = false;
							boolean spaceFoundAfterMapValue = false;
							boolean closeSquareBracketFound = false;
							rstring mapKeyValue =  "";

							// Let us ensure that we see all numerals until we see a
							// close square bracket ].
							while(intMapKey == true && idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterMapValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the map key value.
									if(mapKeyValue != "") {
										spaceFoundAfterMapValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// Since it is an integer map key, we can allow
									// negative sign as the first character of the key.
									if(mapKeyValue == "" && myBlob[idx] == '-') {
										// We can allow this.
										mapKeyValue = "-";
									} else {
										// A non-numeral character found.
										allNumeralsFound = false;
										break;
									}
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterMapValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating int map key.

							if(intMapKey == true && spaceFoundAfterMapValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_INT_MAP_KEY;
								return(false);
							}

							if(intMapKey == true && allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_IN_INT_MAP_KEY;
								return(false);
							}

							if(intMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_INT_MAP_KEY;
								return(false);
							}

							// Let us ensure that we see all numerals with a single
							// decimal point until we see a close square bracket ].
							while(floatMapKey == true && idx < stringLength) {
								if(myBlob[idx] == ']') {
									// Reset this flag.
									spaceFoundAfterMapValue = false;
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ') {
									// We will skip all spaces between [].
									// We will allow spaces only before and after
									// the map key value.
									if(mapKeyValue != "") {
										spaceFoundAfterMapValue = true;
									}

									idx++;
									continue;
								} else if(myBlob[idx] < '0' || myBlob[idx] > '9') {
									// If it is not a numeral, we can only allow
									// only one decimal point either in the beginning or
									// in the middle or in the end of the given numeral value.
									if(myBlob[idx] == '.' ) {
										if(decimalPointCnt < 1) {
											// One decimal point is allowed for a float map key.
											decimalPointCnt++;
											// Add this decimal point to the map key value.
											mapKeyValue += ".";
										} else {
											// We are seeing more than one decimal point.
											// This is not allowed.
											decimalPointCnt++;
											break;
										}
									} else {
										// Since it is a float, we can allow - sign as the
										// first character in the key.
										if(mapKeyValue == "" && myBlob[idx] == '-') {
											mapKeyValue = "-";
										} else {
											// A non-numeral and a non-decimal point found.
											allNumeralsFound = false;
											break;
										}
									}
								} else {
									// If we are observing spaces in between numeral
									// characters, then that is not valid.
									if(spaceFoundAfterMapValue == true) {
										allNumeralsFound = false;
										break;
									}

									allNumeralsFound = true;
									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating float map key.

							if(floatMapKey == true && spaceFoundAfterMapValue == true) {
								error = SPACE_MIXED_WITH_NUMERALS_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && allNumeralsFound == false) {
								error = ALL_NUMERALS_NOT_FOUND_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && decimalPointCnt == 0) {
								// Missing decimal point in the float map key.
								error = MISSING_DECIMAL_POINT_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && decimalPointCnt > 1) {
								// More than one decimal point in the float map key.
								error = MORE_THAN_ONE_DECIMAL_POINT_IN_FLOAT_MAP_KEY;
								return(false);
							}

							if(floatMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_FLOAT_MAP_KEY;
								return(false);
							}

							// Let us ensure that we see a quoted string until we see a
							// close square bracket ].
							while(stringMapKey == true && idx < stringLength) {
								if(closeQuoteFound == true && myBlob[idx] == ']') {
									// We will allow a close square bracket after a
									// close quote has already been seen.
									closeSquareBracketFound = true;
									break;
								}

								if(myBlob[idx] == ' ' &&
									(openQuoteFound == false || closeQuoteFound == true)) {
									// We will skip all spaces between [].
									idx++;
									continue;
								} else if(myBlob[idx] == '"' || myBlob[idx] == '\'') {
									if(openQuoteFound == false) {
										openQuoteFound = true;
									} else if(closeQuoteFound == false) {
										// We can have single or double quote characters as
										// part of the map key. So, we can consider this as
										// an end of map key string's close quote only if it
										// appears at the very end of the map key string.
										if(isQuoteCharacterAtEndOfMapKeyString(myBlob, idx) == true) {
											closeQuoteFound = true;
										} else {
											// It is an embedded quote character.
											// We will add it to our map key value.
											mapKeyValue += myBlob[idx];
										}
										// It will continue the while loop after the
										// end of this if conditional code block.
									} else {
										stringCharacterFoundAfterCloseQuote = true;
										break;
									}
								} else if(myBlob[idx] < ' ' || myBlob[idx] > '~') {
									// A non-string character found.
									invalidStringCharacterFound = true;
									break;
								} else {
									// If we already encountered open and close quotes,
									// then we can't allow any other characters as
									// part of this string.
									if(openQuoteFound == true && closeQuoteFound == true) {
										stringCharacterFoundAfterCloseQuote = true;
										break;
									}

									// If we see a map key character before encountering
									// an open quote character, then it is invalid.
									if(openQuoteFound == false) {
										break;
									}

									mapKeyValue += myBlob[idx];
								}

								idx++;
							} // End of inner while loop validating string map key.

							if(stringMapKey == true && openQuoteFound == false) {
								error = MISSING_OPEN_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && closeQuoteFound == false) {
								error = MISSING_CLOSE_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && invalidStringCharacterFound == true) {
								// This is a very rare error.
								error = INVALID_CHAR_FOUND_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && stringCharacterFoundAfterCloseQuote == true) {
								// This is a very rare error.
								error = CHAR_FOUND_AFTER_CLOSE_QUOTE_IN_STRING_MAP_KEY;
								return(false);
							}

							if(stringMapKey == true && closeSquareBracketFound == false) {
								error = CLOSE_SQUARE_BRACKET_NOT_FOUND_IN_STRING_MAP_KEY;
								return(false);
							}

							// If user gave an empty map key, that is invalid.
							if(mapKeyValue == "") {
								error = EMPTY_STRING_MAP_KEY_FOUND;
								return(false);
							}

							// This map attribute is now fully validated.
							// Move past the ] character.
							idx++;
							// We can insert the list index into our attribute name layout list.
							Functions::Collections::appendM(attributeNameLayoutList,
								mapKeyValue);
							lhsSubscriptForListAndMapAdded = true;
						} // End of if block checking map attribute's [key]

						// We would not have added anything to the attribute name layout list after we added
						// the attribute Name and attributeType if we encountered anything other than a
						// list or map. In that case, we will add an empty string for
						// all the data types other than list and map. Because, they don't use
						// subscript to refer to an lhs value.
						if(lhsSubscriptForListAndMapAdded == false) {
							rstring str = "";
							Functions::Collections::appendM(attributeNameLayoutList, str);
						}

						// Before we wrap this, we must check one more thing to
						// ensure that there is no other non-space character
						// present after the attribute name in the attribute name string.
						while(idx < stringLength) {
							// Skip any space characters which are allowed after a
							// correct attribute name is found and validated.
							if(myBlob[idx] != ' ') {
								// A non-space character found after the fully
								// validated attribute name which is not valid.
								error = NON_SPACE_CHARACTER_FOUND_AFTER_A_VALID_ATTRIBUTE_NAME;
								return(false);
							}

							idx++;
						} // End of inner while loop.

						// We completed the validation of the lhs.
						lhsFound = true;
						// We can break out of  the inner while loop.
						break;
					} // End of the if block that validated the matching tuple attribute.

					// There was no attribute match starting at the current position.
					// Continue to iterate the while loop.
					it++;
				} // End of the inner while loop iterating over the tuple attributes map.

				if(lhsFound == false) {
					error = LHS_NOT_MATCHING_WITH_ANY_TUPLE_ATTRIBUTE;
					return(false);
				}

				if(trace == true) {
					cout << "==== BEGIN eval_predicate trace 1c ====" << endl;
					cout << "Attribute name=" << attributeName << endl;
					cout << "Validation start index=" << validationStartIdx << endl;
					cout <<  "Attribute name layout list after validating an attribute name." << endl;
	    			ConstListIterator it = attributeNameLayoutList.getBeginIterator();

	    			while (it != attributeNameLayoutList.getEndIterator()) {
	    				ConstValueHandle myVal = *it;
	    				rstring const & myString = myVal;
	    				cout << myString << endl;
	    				it++;
	    			} // End of list iteration while loop.

					cout << "==== END eval_predicate trace 1c ====" << endl;
				} // End of if(trace == true)

				break;
    		} // End of if(lhsFound == false)
    	} // End of the outer while loop

    	if(lhsFound == true) {
    		// We successfully validated the given tuple attribute name.
    		//
			// If we are on a recursive call to this method, it usually
			// happens to validate only a tuple attribute that
			// involves list<TUPLE>. In a recursive call,
    		// validationStartIdx will have a non-zero value.
    		// In that case, let us do this special thing.
			if(validationStartIdx > 0) {
				// It is necessary to set the validationStartIdx where
				// we left off before returning from here. That will
				// give the recursive caller a new position in the
				// expression from where to continue if any further
				// validation is needed.
				validationStartIdx = idx;
			}

    		return(true);
    	} else if(Functions::Collections::size(attributeNameLayoutList) == 0) {
    		// This can happen when the entire attribute name string simply
    		// contains space characters without any other characters.
    		error = ATTRIBUTE_NAME_WITH_NO_VALID_CHARACTERS;
    	} else {
    		error = ATTRIBUTE_NAME_NOT_GOOD_FOR_VALIDATION;
    	}

    	return(false);
    } // End of validateTupleAttributeName

    // This method gets the value held in a given tuple by
    // a given attribute name.
    template<class T1, class T2>
    inline void fetchTupleAttributeValue(rstring const & attributeName,
    	SPL::map<rstring, rstring> const & tupleAttributesMap,
		SPL::list<rstring> const & attributeNameLayoutList,
		T1 const & myTuple, T2 & value, int32 & error, boolean trace) {
    	// This method will get called recursively when a list<TUPLE> is
    	// encountered in a given attribute name. It is important to note that
    	// the recursive caller must always pass its own newly formed
    	// local variables as method arguments here so as not to overwrite
    	// the original reference pointers passed by the very first caller
    	// who made the initial non-recursive call.
    	error = ALL_CLEAR;

    	// We can find everything we need to get the value of a given
    	// attribute in the arguments passed to this method.
		//
        // Example tuple attribute names:
        // "symbol"
    	// "employee.skills"
    	// "stats.numberOfSchools"
    	// "details.location.geo.latitude"
    	// "details.location.info.officials"
        //
		// The attribute name layout list describes the composition of a given attribute name.
		// Structure of such a list will go something like this:
		// This list will have a sequence of rstring items as shown below.
		// LHSAttribName
		// LHSAttribType
		// ListIndexOrMapKeyValue  - When N/A, it will have an empty string.
    	// Optional entry for list<TUPLE> attribute name start index - When N/A, this entry will not be there.
    	// Optional entry for list<TUPLE> attribute name end index - When N/A, this entry will not be there.
    	//
		int32 attributeNameLayoutListCnt =
			Functions::Collections::size(attributeNameLayoutList);

		if(attributeNameLayoutListCnt == 0) {
			// It is very rare for this to happen. But, we will check for it.
			error = EMPTY_ATTRIBUTE_NAME_LAYOUT_LIST_DURING_VALUE_FETCH;
			return;
		}

		// We have only one attribute name whose value needs to be obtained.
		// We can read different aspects of a given attribute name from the
		// attribute name layout list. That list was sent here as a method argument.
		//
		int32 idx = 0;
		// Get the LHS attribute name.
		rstring lhsAttributeName = attributeNameLayoutList[idx++];
		// Get the LHS attribute type.
		rstring lhsAttributeType = attributeNameLayoutList[idx++];
		// Get the list index or the map key value.
		rstring listIndexOrMapKeyValue = attributeNameLayoutList[idx++];

		if(trace == true) {
			cout << "==== BEGIN eval_predicate trace 2c ====" << endl;
			cout << "Attribute name=" << lhsAttributeName << endl;
			cout <<  "Attribute name layout list before fetching an attribute value." << endl;
			ConstListIterator it = attributeNameLayoutList.getBeginIterator();

			while (it != attributeNameLayoutList.getEndIterator()) {
				ConstValueHandle myVal = *it;
				rstring const & myString = myVal;
				cout << myString << endl;
				it++;
			} // End of list iteration while loop.

			cout << "==== END eval_predicate trace 2c ====" << endl;
		}

		ConstValueHandle cvh;
		// Get the constant value handle for this attribute.
		getConstValueHandleForTupleAttribute(myTuple, lhsAttributeName, cvh);

		try {
    		// Depending on the LHS attribute type, we will now
        	// fetch the value of a given attribute in a given tuple.
			// ****** rstring, indexed list and map based rstring value fetches ******
			if(lhsAttributeType == "rstring") {
				value = cvh;
			} else if(lhsAttributeType == "list<rstring>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::list<rstring> const & myList = cvh;
				// Check if the user given list index is valid.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 ||
					listIdx > (Functions::Collections::size(myList) - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				rstring const & myVal = myList[listIdx];
				value = ConstValueHandle(myVal);
			} else if(lhsAttributeType == "map<int32,rstring>" &&
				listIndexOrMapKeyValue != "") {
				SPL::map<int32,rstring> const & myMap = cvh;
				// Check if the user given map key is valid.
				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				// Using the [] map element access operator with a
				// const SPL::map variable gives a compiler error
				// saying that [] operation can't be performed using a const SPL::map.
				// So, I'm using the at access method.
				rstring const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
			} else if(lhsAttributeType == "map<int64,rstring>" &&
        		listIndexOrMapKeyValue != "") {
				SPL::map<int64,rstring> const & myMap = cvh;
				// Check if the user given map key is valid.
				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				// Using the [] map element access operator with a
				// const SPL::map variable gives a compiler error
				// saying that [] operation can't be performed using a const SPL::map.
				// So, I'm using the at access method.
				rstring const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
			} else if(lhsAttributeType == "map<float32,rstring>" &&
        		listIndexOrMapKeyValue != "") {
				SPL::map<float32,rstring> const & myMap = cvh;
				// Check if the user given map key is valid.
				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				// The C atof API used above actually returns double which is
				// equivalent to the SPL float64. So, assigning the result from
				// that API to an SPL float32 variable can't be made fully
				// accurate without losing some precision for very large float32 numbers.
				// It is a known limitation in C. So, for very large float32 based map keys,
				// I had problems in the SPL 'has' method where it wrongly returned that
				// the key doesn't exist when it actually is there.
				// e-g: 5.28438e+08
				// So, I stopped calling the SPL 'has' function from C++ code.
				// The following manual procedure by converting the float based key into
				// a string and then comparing it worked for me. I don't know how much
				// overhead it will add compared to the SPL 'has' function if it indeed works.
				ostringstream key;
				key << mapKey;
				boolean keyExists = false;

				ConstMapIterator it = myMap.getBeginIterator();

				while (it != myMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,rstring> const & myFloat32RString = myVal;
					ostringstream firstMember;
					firstMember << myFloat32RString.first;

					if(key.str() == firstMember.str()) {
						keyExists = true;
						rstring const & myVal = myFloat32RString.second;
						value = ConstValueHandle(myVal);
						break;
					}

					it++;
				}

				if(keyExists == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}
			} else if(lhsAttributeType == "map<float64,rstring>" &&
        		listIndexOrMapKeyValue != "") {
				SPL::map<float64,rstring> const & myMap = cvh;
				// Check if the user given map key is valid.
				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				// Using the [] map element access operator with a
				// const SPL::map variable gives a compiler error
				// saying that [] operation can't be performed using a const SPL::map.
				// So, I'm using the at access method.
				rstring const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
			} else if(lhsAttributeType == "map<rstring,rstring>" &&
        		listIndexOrMapKeyValue != "") {
				SPL::map<rstring,rstring> const & myMap = cvh;

				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				// Using the [] map element access operator with a
				// const SPL::map variable gives a compiler error
				// saying that [] operation can't be performed using a const SPL::map.
				// So, I'm using the at access method.
				rstring const & myVal = myMap.at(listIndexOrMapKeyValue);
				value = ConstValueHandle(myVal);
    		// ****** Non-indexed full set, list and map value fetches ******
			} else if(lhsAttributeType == "set<int32>") {
				value = cvh;
			} else if(lhsAttributeType == "set<int64>") {
				value = cvh;
			} else if(lhsAttributeType == "set<float32>") {
				value = cvh;
			} else if(lhsAttributeType == "set<float64>") {
				value = cvh;
			} else if(lhsAttributeType == "set<rstring>") {
				value = cvh;
			} else if(lhsAttributeType == "list<rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "list<int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "list<int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "list<float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "list<float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<rstring,rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<rstring,int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<rstring,int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<rstring,float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<rstring,float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int32,rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int32,int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int32,int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int32,float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int32,float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int64,rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int64,int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int64,int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int64,float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<int64,float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float32,rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float32,int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float32,int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float32,float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float32,float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float64,rstring>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float64,int32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float64,int64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float64,float32>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
			} else if(lhsAttributeType == "map<float64,float64>" &&
				listIndexOrMapKeyValue == "") {
				value = cvh;
        		// ****** Primitive types value fetches ******
			} else if(lhsAttributeType == "int32") {
				value = cvh;
			} else if(lhsAttributeType == "uint32") {
				value = cvh;
			} else if(lhsAttributeType == "int64") {
				value = cvh;
			} else if(lhsAttributeType == "uint64") {
				value = cvh;
			} else if(lhsAttributeType == "float32") {
				value = cvh;
			} else if(lhsAttributeType == "float64") {
				value = cvh;
			} else if(lhsAttributeType == "boolean") {
				value = cvh;
				// ****** Indexed list and map based non-rstring value fetches ******
    		} else if(lhsAttributeType == "list<int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::list<int32> const & myList = cvh;
				// Check if the user given list index is valid.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 ||
					listIdx > (Functions::Collections::size(myList) - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				int32 const & myVal = myList[listIdx];
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "list<int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::list<int64> const & myList = cvh;
				// Check if the user given list index is valid.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 ||
					listIdx > (Functions::Collections::size(myList) - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				int64 const & myVal = myList[listIdx];
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "list<float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::list<float32> const & myList = cvh;
				// Check if the user given list index is valid.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 ||
					listIdx > (Functions::Collections::size(myList) - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				float32 const & myVal = myList[listIdx];
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "list<float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::list<float64> const & myList = cvh;
				// Check if the user given list index is valid.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 ||
					listIdx > (Functions::Collections::size(myList) - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				float64 const & myVal = myList[listIdx];
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<rstring,int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<rstring,int32> const & myMap = cvh;

				// Check if the user given map key is valid.
				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int32 const & myVal = myMap.at(listIndexOrMapKeyValue);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<rstring,int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<rstring,int64> const & myMap = cvh;

				// Check if the user given map key is valid.
				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int64 const & myVal = myMap.at(listIndexOrMapKeyValue);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<rstring,float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<rstring,float32> const & myMap = cvh;

				// Check if the user given map key is valid.
				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float32 const & myVal = myMap.at(listIndexOrMapKeyValue);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<rstring,float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<rstring,float64> const & myMap = cvh;

				// Check if the user given map key is valid.
				if(Functions::Collections::has(myMap, listIndexOrMapKeyValue) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float64 const & myVal = myMap.at(listIndexOrMapKeyValue);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int32,int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int32,int32> const & myMap = cvh;
				// Check if the user given map key is valid.
				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int32,int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int32,int64> const & myMap = cvh;
				// Check if the user given map key is valid.
				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int64,int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int64,int32> const & myMap = cvh;
				// Check if the user given map key is valid.
				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int64,int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int64,int64> const & myMap = cvh;
				// Check if the user given map key is valid.
				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int32,float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int32,float32> const & myMap = cvh;
				// Check if the user given map key is valid.
				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int32,float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int32,float64> const & myMap = cvh;
				// Check if the user given map key is valid.
				int32 const & mapKey = atoi(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int64,float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int64,float32> const & myMap = cvh;
				// Check if the user given map key is valid.
				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<int64,float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<int64,float64> const & myMap = cvh;
				// Check if the user given map key is valid.
				int64 const & mapKey = atol(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<float32,int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float32,int32> const & myMap = cvh;
				// Check if the user given map key is valid.
				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				// The C atof API used above actually returns double which is
				// equivalent to the SPL float64. So, assigning the result from
				// that API to an SPL float32 variable can't be made fully
				// accurate without losing some precision for very large float32 numbers.
				// It is a known limitation in C. So, for very large float32 based map keys,
				// I had problems in the SPL 'has' method where it wrongly returned that
				// the key doesn't exist when it actually is there.
				// e-g: 5.28438e+08
				// So, I stopped calling the SPL 'has' function from C++ code.
				// The following manual procedure by converting the float based key into
				// a string and then comparing it worked for me. I don't know how much
				// overhead it will add compared to the SPL 'has' function if it indeed works.
		    	ostringstream key;
		    	key << mapKey;
		    	boolean keyExists = false;

				ConstMapIterator it = myMap.getBeginIterator();

				while (it != myMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,int32> const & myFloat32Int32 = myVal;
			    	ostringstream firstMember;
			    	firstMember << myFloat32Int32.first;

			    	if(key.str() == firstMember.str()) {
			    		keyExists = true;
			    		int32 const & myVal = myFloat32Int32.second;
			    		value = ConstValueHandle(myVal);
	    				break;
			    	}

					it++;
				}

				if(keyExists == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}
    		} else if(lhsAttributeType == "map<float32,int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float32,int64> const & myMap = cvh;
				// Check if the user given map key is valid.
				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				// The C atof API used above actually returns double which is
				// equivalent to the SPL float64. So, assigning the result from
				// that API to an SPL float32 variable can't be made fully
				// accurate without losing some precision for very large float32 numbers.
				// It is a known limitation in C. So, for very large float32 based map keys,
				// I had problems in the SPL 'has' method where it wrongly returned that
				// the key doesn't exist when it actually is there.
				// e-g: 5.28438e+08
				// So, I stopped calling the SPL 'has' function from C++ code.
				// The following manual procedure by converting the float based key into
				// a string and then comparing it worked for me. I don't know how much
				// overhead it will add compared to the SPL 'has' function if it indeed works.
		    	ostringstream key;
		    	key << mapKey;
		    	boolean keyExists = false;

				ConstMapIterator it = myMap.getBeginIterator();

				while (it != myMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,int64> const & myFloat32Int64 = myVal;
			    	ostringstream firstMember;
			    	firstMember << myFloat32Int64.first;

			    	if(key.str() == firstMember.str()) {
			    		keyExists = true;
			    		int64 const & myVal = myFloat32Int64.second;
			    		value = ConstValueHandle(myVal);
			    		break;
			    	}

					it++;
				}

				if(keyExists == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}
    		} else if(lhsAttributeType == "map<float64,int32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float64,int32> const & myMap = cvh;
				// Check if the user given map key is valid.
				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<float64,int64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float64,int64> const & myMap = cvh;
				// Check if the user given map key is valid.
				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				int64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<float32,float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float32,float32> const & myMap = cvh;
				// Check if the user given map key is valid.
				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				// The C atof API used above actually returns double which is
				// equivalent to the SPL float64. So, assigning the result from
				// that API to an SPL float32 variable can't be made fully
				// accurate without losing some precision for very large float32 numbers.
				// It is a known limitation in C. So, for very large float32 based map keys,
				// I had problems in the SPL 'has' method where it wrongly returned that
				// the key doesn't exist when it actually is there.
				// e-g: 5.28438e+08
				// So, I stopped calling the SPL 'has' function from C++ code.
				// The following manual procedure by converting the float based key into
				// a string and then comparing it worked for me. I don't know how much
				// overhead it will add compared to the SPL 'has' function if it indeed works.
		    	ostringstream key;
		    	key << mapKey;
		    	boolean keyExists = false;

				ConstMapIterator it = myMap.getBeginIterator();

				while (it != myMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,float32> const & myFloat32Float32 = myVal;
			    	ostringstream firstMember;
			    	firstMember << myFloat32Float32.first;

			    	if(key.str() == firstMember.str()) {
			    		keyExists = true;
			    		float32 const & myVal = myFloat32Float32.second;
			    		value = ConstValueHandle(myVal);
			    		break;
			    	}

					it++;
				}

				if(keyExists == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}
    		} else if(lhsAttributeType == "map<float32,float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float32,float64> const & myMap = cvh;
				// Check if the user given map key is valid.
				float32 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				// The C atof API used above actually returns double which is
				// equivalent to the SPL float64. So, assigning the result from
				// that API to an SPL float32 variable can't be made fully
				// accurate without losing some precision for very large float32 numbers.
				// It is a known limitation in C. So, for very large float32 based map keys,
				// I had problems in the SPL 'has' method where it wrongly returned that
				// the key doesn't exist when it actually is there.
				// e-g: 5.28438e+08
				// So, I stopped calling the SPL 'has' function from C++ code.
				// The following manual procedure by converting the float based key into
				// a string and then comparing it worked for me. I don't know how much
				// overhead it will add compared to the SPL 'has' function if it indeed works.
		    	ostringstream key;
		    	key << mapKey;
		    	boolean keyExists = false;

				ConstMapIterator it = myMap.getBeginIterator();

				while (it != myMap.getEndIterator()) {
					std::pair<ConstValueHandle, ConstValueHandle> myVal = *it;
					std::pair<float32,float64> const & myFloat32Float64 = myVal;
			    	ostringstream firstMember;
			    	firstMember << myFloat32Float64.first;

			    	if(key.str() == firstMember.str()) {
			    		keyExists = true;
			    		float64 const & myVal = myFloat32Float64.second;
			    		value = ConstValueHandle(myVal);
			    		break;
			    	}

					it++;
				}

				if(keyExists == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}
    		} else if(lhsAttributeType == "map<float64,float32>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float64,float32> const & myMap = cvh;
				// Check if the user given map key is valid.
				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float32 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
    		} else if(lhsAttributeType == "map<float64,float64>" &&
    			listIndexOrMapKeyValue != "") {
				SPL::map<float64,float64> const & myMap = cvh;
				// Check if the user given map key is valid.
				float64 const & mapKey = atof(listIndexOrMapKeyValue.c_str());

				if(Functions::Collections::has(myMap, mapKey) == false) {
					error = INVALID_KEY_FOR_LHS_MAP_ATTRIBUTE;
					return;
				}

				float64 const & myVal = myMap.at(mapKey);
				value = ConstValueHandle(myVal);
				// ****** list<TUPLE> value fetches ******
    		} else if(Functions::String::findFirst(lhsAttributeType, "list<tuple<") == 0  &&
				listIndexOrMapKeyValue == "") {
				// This is a list<TUPLE> with no index value.
				// Caller wants to get the entire list holding all the tuples.
				// The line below shows an example of an attribute type (schema)
				// followed by an attribute name.
				// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList
				value = cvh;
			} else if(Functions::String::findFirst(lhsAttributeType, "list<tuple<") == 0  &&
				listIndexOrMapKeyValue != "") {
				// If it is a list<TUPLE> with an index value, this needs a special value fetch.
				// e-g: Body.MachineStatus.ComponentList[0].Component["MapKey"] == "MapValue"
				// This list doesn't hold items that are made of
				// well defined SPL built-in data types.
				// Instead, it holds a user defined Tuple type. So, we can't
				// adopt the technique we used in the other else blocks above
				// to assign cvh to a variable such as SPL::list<rstring>.
				// We must use the C++ interface type SPL::List that the
				// SPL::list is based on.
				// The line below shows an example of an attribute type (schema)
				// followed by an attribute name.
				// e-g: list<tuple<map<rstring,rstring> Component>> ComponentList
				SPL::List const & myListTuple = cvh;
				ConstListIterator it = myListTuple.getBeginIterator();

				// We have to look only for a tuple that is present in the
				// user specified list index. Let us first check if that
				// index is valid in a given list.
				int32 listIdx = atoi(listIndexOrMapKeyValue.c_str());

				if(listIdx < 0 || listIdx > (myListTuple.getSize() - 1)) {
					error = INVALID_INDEX_FOR_LHS_LIST_ATTRIBUTE;
					return;
				}

				// Use of "lot" in the local variable names below means List Of Tuple.
				int32 lotIdx = -1;
				boolean lotResult = false;
				SPL::map<rstring, rstring> lotTupleAttributesMap;
				int32 lotError = 0;
				rstring lotTupleSchema = "";

				while (it != myListTuple.getEndIterator()) {
					// We have to process only the tuple held in a
					// user given list index value in the attribute name.
					// Remaining list items can be skipped.
					lotIdx++;

					if(lotIdx != listIdx) {
						// Continue the while loop.
						it++;
						continue;
					}

					ConstValueHandle myVal = *it;
					Tuple const & lotTuple = myVal;

					// We can now get the attributes present inside this tuple.
					//
					int32 lotSchemaLength =
						Functions::String::length(lhsAttributeType);

					// Get just the tuple<...> part from this attribute's type.
					// We have to skip the initial "list<" portion in the type string.
					// We also have to skip the final > in the type string that
					// is a closure for "list<" i.e. we start from the
					// zero based index 5 and take the substring until the
					// end except for the final > which in total is 6 characters
					// less than the length of the entire type string.
					lotTupleSchema = Functions::String::substring(lhsAttributeType,
						5, lotSchemaLength-6);
					lotResult = parseTupleAttributes(lotTupleSchema,
						lotTupleAttributesMap, lotError, trace);

					if(lotResult == false) {
						// This error should never happen since we
						// already passed this check twice even before
						// coming into this value fetch method.
						// We are simply doing it here as a safety measure.
						error = ATTRIBUTE_PARSING_ERROR_IN_LIST_OF_TUPLE_VALUE_FETCH;

						if(trace == true) {
							cout << "It failed to get the list<TUPLE> attributes for " <<
								lhsAttributeName <<
								" during the tuple attribute value fetch. Error=" << lotError <<
								". Tuple schema=" << lotTupleSchema << endl;
						}

						// Stop fetching value of list<TUPLE> due to this rare error.
						break;
					}

					// We got the LOT tuple attributes.
					// At this time, we have to get the attribute name layout list for
					// the attribute name involving a list<TUPLE>. Unlike we
					// normally do it in the validation method for the
					// SPL built-in data types based attribute names,
					// we don't prepare it for a list<TUPLE> based
					// attribute name and save it ahead of time in the
					// attribute name layout list. So, we now have to make a straight
					// non-recursive call to the the validation method which
					// will get us an attribute name layout list that we can use
					// here only once for fetching the value of a list<TUPLE> based
					// attribute name.
					//
					// We have to parse just a partial portion of the full
					// attribute name string that is passed to this method.
					// In the normal attribute name validation step earlier,
					// we have stored the start index and end index for
					// that partial string portion we need below.
					// You can refer back to the attribute name validation method to see
					// how we store these two indices. In essence,
					// the final two entries of the attribute name layout list
					// already contain these two numbers in the case of a list<TUPLE>.
					// They are in zero based list index 3 and 4.
					int32 startIdx = atoi(attributeNameLayoutList[3].c_str());
					int32 endIdx = atoi(attributeNameLayoutList[4].c_str());
					// We can take a substring to get the
					// attribute portion that we are interested in.
					rstring lotAttributeName = Functions::String::substring(
						attributeName, startIdx, (endIdx-startIdx+1));

					if(trace == true) {
						cout << "LOT attribute name in value fetch=" << lotAttributeName << endl;
					}

					SPL::list<rstring> lotAttributeNameLayoutList;
					// We are making a non-recursive call. So, set it to 0.
					// i.e. start validating from index 0 of the
					// lot attribute name string we created above using substring.
					int32 validationStartIdx = 0;
					lotResult = validateTupleAttributeName(lotAttributeName,
						lotTupleAttributesMap, lotAttributeNameLayoutList,
						error, validationStartIdx, trace);

					if(lotResult == false) {
						// There was a validation error.
						// This should be rare as we have done this successfully
						// once already in the normal validation step earlier.
						if(trace == true) {
							cout << "LOT validation error during tuple attribute value fetch. Error=" << error << endl;
						}

						break;
					}

					// We can recursively call the current
					// method that we are in now to fetch the
					// tuple attribute value involving a list<TUPLE>.
					if(trace == true) {
						cout << "BEGIN Recursive fetch tuple attribute value call for list<TUPLE> " <<
							attributeName << "." << endl;
					}

					// Call the same method we are in now.
				    fetchTupleAttributeValue(lotAttributeName, lotTupleAttributesMap,
				    	lotAttributeNameLayoutList, lotTuple, value, error, trace);

					if(trace == true) {
						cout << "END Recursive fetch tuple attribute value call for list<TUPLE> " <<
							lhsAttributeName << "." << endl;
					}

					// We are done fetching the attribute value of the single tuple from this
					// list<TUPLE> that the user specified in the attribute name.
					break;
				} // End of while loop
			} else {
				// Unsupported fetch attribute value condition.
				error = UNSUPPORTED_FETCH_ATTRIBUTE_VALUE_CONDITION_DETECTED;
			} // End of the many if conditional blocks for different value fetches.
        } catch(...) {
        	// This exception is most likely related to the
        	// tuple attribute value assignment being done to a
        	// method argument passed by the caller that has a
        	// wrong data type.
        	error = WRONG_TYPE_OF_ATTRIBUTE_PASSED_AS_FUNCTION_ARGUMENT_BY_CALLER;
        }

        if(error != ALL_CLEAR) {
        	return;
        }

		if(trace == true) {
			cout << "==== BEGIN eval_predicate trace 3c ====" << endl;
			cout << "Attribute name=" << attributeName <<
				" with a type of " << lhsAttributeType <<
				" was fetched successfully and being returned to the caller." << endl;

			cout << "==== END eval_predicate trace 3c ====" << endl;
		}
    } // End of fetchTupleAttributeValue
    // ====================================================================
} // End of namespace eval_predicate_functions
// ====================================================================

/*
===============================================================================
Coda
----
Whatever path lies ahead for IBM Streams beyond 2Q2021, I'm proud to have
played a key role in this product team from 2007 to 2021. IBM Streams
gave me marvelous opportunities to create beautiful extensions, build
cutting edge streaming analytics solutions, coach/train top-notch customers
and co-create meaningful production-grade software assets with them. Thus far,
it formed the best period in my 36 years of Software career. I created
this challenging Rule Processing toolkit for meeting a critical business need
of a prestigious customer in the semiconductor industry. Most likely, it is the
last hurrah in my technical contributions to the incomparable IBM Streams.
I will be thrilled to get another chance to associate with this wonderful
product if it finds a new home for further development either inside or
outside of IBM. Until then, I will continue to reminisce this unforgettable
journey made possible by the passionate researchers, engineers and managers
who are all simply world class.

It will take many more years for some other company or an open-source group to
roll out a full-featured product that can be a true match for IBM Streams.

***** Let the pioneering concepts of IBM Streams shine forever. *****

Yours truly,
SN (June 2021)
===============================================================================
*/

#endif
