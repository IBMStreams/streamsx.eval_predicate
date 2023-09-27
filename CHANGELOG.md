# Changes

## v1.1.7
* Sep/27/2023
* Made a fix to correctly evaluate a new multi-level nested expression pattern as shown in the test case A51.21 in FunctionalTests.spl.

## v1.1.6
* Sep/20/2023
* Made a major redesign to evaluate multi-level nested subexpressions.
* Made plenty of code changes in the expression validation and evaluation sections.
* Added many new trace points to follow the code execution path when troubleshooting.
* Added 14 new multi-level nested subexpression test cases in FunctionalTests.spl.

## v1.1.5
* Jan/10/2023
* Added inline to the C++ function get_tuple_schema_and_attribute_info.

## v1.1.4
* Jan/07/2023
* Added a new function get_tuple_schema_and_attribute_info to fetch the tuple schema literal string along with the tuple attribute information map with fully qualified tuple attribute names and their SPL type names as key/value pairs in that map. 

## v1.1.3
* Oct/16/2021
* Enhanced the compare_tuple_attributes function to give back two lists i.e. one with the attribute names that have matching values and another with the attribute names that have differing values.

## v1.1.2
* Oct/16/2021
* Added a new function compare_tuple_attributes to compare the attribute values of two tuples that are based on the same schema and then give back a list containing the attribute names that have differing values in the two tuples being compared.

## v1.1.1
* Sep/04/2021
* Added two new operational verbs: equalsCI and notEqualsCI.

## v1.1.0
* June/07/2021
* Modified it to support the in operational verb for int32, float64 and rstring based tuple attributes and the inCI operational verb only for rstring based tuple attributes.

## v1.0.9
* June/01/2021
* Added two new operational verbs: in and inCI.

## v1.0.8
* May/23/2021
* Added more commentary and fixed a few typos.

## v1.0.7
* May/05/2021
* Made all C++ functions as inline.
* Added support for lexical comparison of string attributes with RHS string values.

## v1.0.6
* Apr/29/2021
* Added support for string values inside a rule expression to have parenthesis and square bracket characters mixed with other characters without impacting the parenthesis and square bracket matching in other parts of the rule expression.

## v1.0.5
* Apr/25/2021
* Added new operational verbs containsCI, startsWithCI, endsWithCI, notContainsCI, notStartsWithCI and notEndsWithCI for case insensitive (CI) string operations.
* Added support for relational operators to be used with string based attributes.
* Added support for string values inside a rule expression to have single or double quote characters mixed with other characters.

## v1.0.4
* Apr/23/2021
* Added a new function get_tuple_attribute_value to fetch the value of a user given attribute name if present in a user given tuple.

## v1.0.3
* Apr/20/2021
* Added more special operations for rstring, set, list and map. They are sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT and sizeGE.

## v1.0.2
* Apr/15/2021
* Added support for the use of list of TUPLE in a rule expression.

## v1.0.1
* Apr/12/2021
* Added support for multilevel parenthesis i.e. nested rule expressions.

## v1.0.0
* Apr/05/2021
* Very first release of this toolkit that was tested to support the use of set, list, map and nested tuple attributes in a rule expression.
