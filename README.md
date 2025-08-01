# Rule processor for IBM Streams

## Note from the toolkit author
This toolkit created by Senthil Nathan is a value differentiator for key customers. He created it on his own initiative and with his original intellectual ideas and not out of any formally managed or funded work by IBM. He did the entire work needed for this toolkit much after IBM divested its Streams product to another company around May/2021 as well as after Oct/2024 when he left IBM. He is now an independent software consultant. To benefit from the compelling features of this asset, for any enhancements, any need for a new powerful toolkit as well as for creating new streaming data analytics solutions, customers can email senthil@moonraytech.com to reach him. Thank you.

## Purpose
This toolkit offers an improved and a simpler facility for users to let their externally authored business rules to be consumed either statically or dynamically from within the IBM Streams SPL application code and then process (evaluate) them as the data flows through the application pipeline. Such an evaluation returns a true or false result for every rule that gets processed to indicate whether the rule expression criteria is met or not met.

## Major features
It provides the following function that can be called from within the IBM Streams application code. This function is designed to use internal in-memory caching in order to be optimal when processing a rule that gets evaluated frequently inside a given IBM Streams Custom operator logic. Such caching is done on a per PE i.e. Linux process thread basis. In addition, it uses C++ reference pointers wherever possible to reduce the overall rule processing time by avoiding unnecessary data copy that can get expensive. 

**eval_predicate** is a C++ native function provided via this toolkit. IBM Streams applications can add this toolkit as a dependency either via the -t Streams compiler option or by adding this toolkit directory path in the Streams Studio and/or the Microsoft Visual Studio Code. Once the dependency is in place, this function can be called from wherever it is needed within the application code.

```
// This namespace usage declaration is needed at the top of an application.
use com.ibm.streamsx.eval_predicate::*;

// Schema can include other deeply nested types.
// But, a simple flat tuple schema is used here for illustration.
type Ticker_t = rstring symbol, float32 price, uint32 quantity, 
   boolean buyOrSell, map<boolean, int32> myMap1;

// Let us say that an incoming tuple holds the values as shown below. 
// A given rule expression will get evaluated (processed) based on a 
// user given tuple's attribute values.
mutable Ticker_t myTicker = {};
myTicker.symbol = "INTC";
myTicker.price = (float32)79.25;
myTicker.quantity = 1287u;
myTicker.buyOrSell = true;
myTicker.myMap1 = {true:1, false:0};

// Following code snippet shows how a Custom operator logic 
// can do the rule processing.
mutable int32 error = 0;
rstring rule27 = "(symbol == 'INTC' && price > 698.56 && quantity == 7492)";
boolean result = eval_predicate(rule27, myTicker, error, false);
					
if(result == true) {
   printStringLn("Rule 27: Evaluation criteria is met.");
} else if(result == false && error == 0) {
   printStringLn("Rule 27: Evaluation criteria is not met.");
} else {
   printStringLn("Rule 27: Evaluation execution failed. Error=" + (rstring)error);
}

// Following is the usage description for the eval_predicate function.
//
// Arg1: Expression string i.e. your custom rule.
// Arg2: Your tuple carrying the attributes needed for evaluating the rule.
// Arg3: A mutable int32 variable to receive a non-zero eval error code if any.
// (You can refer to top of the impl/include/eval_predicate.h file in the 
// streamsx.eval_predicate toolkit for the meaning of a given error code.)
// Arg4: A boolean value to enable debug tracing inside this function.
// It returns true if the rule evaluation criteria is met.
// It returns false and error=0 if the rule evaluation criteria is not met.
// It returns a non-zero error when there is an evaluation execution failure.
```

**get_tuple_attribute_value** is another C++ native function provided via this toolkit. This function fetches the value of a user given attribute name if present in a user given tuple. Any valid fully qualified attribute name can be given via a input string argument to this function.

```
// This namespace usage declaration is needed at the top of an application.
use com.ibm.streamsx.eval_predicate::*;

// Schema can include other deeply nested types.
type Person_t = rstring name, rstring title, 
   int32 id, rstring gender, set<rstring> skills;
type Department_t = rstring name, rstring id, 
   rstring manager, int32 employeeCnt;
type Employee_t = Person_t employee, Department_t department;

// Let us say that an incoming tuple holds the values as shown below. 
mutable Employee_t myEmployee = {};
myEmployee.employee.name = "Jill Doe";
myEmployee.employee.title = "Software Engineer";
myEmployee.employee.id = 452397;
myEmployee.employee.gender = "Female";
myEmployee.employee.skills = {"C++", "Java", "Python", "SPL"};
myEmployee.department.name = "Process Optimization";
myEmployee.department.id = "XJ825";
myEmployee.department.manager = "Mary Williams";
myEmployee.department.employeeCnt = 18;

// Following code snippet shows how a Custom operator logic 
// can fetch the value of a given tuple attribute.
mutable int32 error = 0;
// Get the full set<rstring> value of a given tuple attribute name.
rstring attributeName = "employee.skills";
mutable set<rstring> employeeSkills = {};
get_tuple_attribute_value(attributeName, myEmployee,
   employeeSkills, error, false);
					
if(error == 0) {
   printStringLn("Tuple attribute value was fetched successfully.");				
   printStringLn(attributeName + "=" + (rstring)employeeSkills);
} else {
   printStringLn("Tuple attribute value was not fetched successfully. Error=" + (rstring)error);
}

// Following is the usage description for the get_tuple_attribute_value function.
//
// Arg1: Fully qualified attribute name
// Arg2: Your tuple
// Arg3: A mutable variable of an appropriate type in which the
//       value of a given attribute will be returned.
// Arg4: A mutable int32 variable to receive non-zero error code if any.
// Arg5: A boolean value to enable debug tracing inside this function.
// It is a void function that returns nothing.
```

**compare_tuple_attributes** is another C++ native function provided via this toolkit. This function compares the attribute values of two tuples that are based on the same schema. It will give back a list containing attribute names that have matching values and another list containing attribute names that have differing values in the two tuples being compared. It supports primitive types (int32, float64, rstring etc.) as well as collection types (set, list and map). It allows flat tuples as well as deeply nested tuples to be compared.

```
// This namespace usage declaration is needed at the top of an application.
use com.ibm.streamsx.eval_predicate::*;

// Schema can include other deeply nested types.
type NestedType1_t = int32 x, float64 y, rstring z, list<int32> l;
type NestedType2_t = NestedType1_t m, map<int32, boolean> n, timestamp o;
type Test_t = boolean a, int32 b, uint64 c, float32 d, float64 e,
              rstring f, set<int32> g, list<rstring> h, map<int32, rstring> i,
              map<rstring, float64> j, NestedType2_t k;
					
// Create two tuples that are based on the same schema.
mutable Test_t myTuple1 = {};
mutable Test_t myTuple2 = {};
mutable list<rstring> matchingAttributes = [];
mutable list<rstring> differingAttributes = [];
					
// Populate the tuple with some data.
myTuple1.a = true;
myTuple1.b = 456;
myTuple1.c = 789ul;
myTuple1.d = (float32)123.45;
myTuple1.e = 987.65;
myTuple1.f = "Life is mainly there to have fun.";
myTuple1.g = {5, 9, 2, 6};
myTuple1.h = ['One', 'Two', 'Three'];
myTuple1.i = {1:'One', 2:'Two', 3:'Three'};
myTuple1.j = {"One":1.0, "Two":2.0, "Three":3.0};
myTuple1.k.m.x = 678;
myTuple1.k.m.y = 936.27;
myTuple1.k.m.z = "String inside a nested tuple.";
myTuple1.k.m.l = [67, 78, 89];
myTuple1.k.n = {1:true, 2:false, 3:true}; 
myTuple1.k.o = getTimestamp();
						
// Make the second tuple same as the first tuple.
myTuple2 = myTuple1;
// Make some changes to certain attribute values in the second tuple.
myTuple2.a = false;
myTuple2.d = (float32)145.12;
myTuple2.f = "Life is mainly there to have joy and peace.";
myTuple2.i = {10:'Ten', 9:'Nine', 8:'Eight'};
myTuple2.k.m.y = 27.93;
myTuple2.k.m.z = "Different string inside a nested tuple.";
myTuple2.k.n = {1:true, 2:true, 3:true};
// Wait for 2 seconds for time to change.
block(2.0);
myTuple2.k.o = getTimestamp(); 
					
// Compare them now.
compare_tuple_attributes(myTuple1, myTuple2, 
   matchingAttributes, differingAttributes, 
   error, $EVAL_PREDICATE_TRACING);
	    			
if(error == 0) {
   printStringLn("Compare tuple attributes function returned successfully. " +
      "matchingAttributes = " + (rstring)matchingAttributes + 
      ", differingAttributes = " + (rstring)differingAttributes);
} else {
   printStringLn("Compare tuple attributes function returned an error. Error=" + 
     (rstring)error);
}
					}
// Following is the usage description for the compare_tuple_attributes function.
//
// Arg1: Your tuple1
// Arg2: Your tuple2
// Arg3: A mutable variable of list<string> type in which the
//       attribute names that have a match in their values will be returned.
// Arg4: A mutable variable of list<string> type in which the
//       attribute names that differ in their values will be returned.
// Arg5: A mutable int32 variable to receive non-zero error code if any.
// Arg6: A boolean value to enable debug tracing inside this function.
// It is a void function that returns nothing.
```

**get_tuple_schema_and_attribute_info** is another C++ native function provided via this toolkit. This function fetches the tuple schema literal string along with the tuple attribute information as a map with fully qualified tuple attribute names and their SPL type names as key/value pairs in that map.

```
// This namespace usage declaration is needed at the top of an application.
use com.ibm.streamsx.eval_predicate::*;

mutable rstring tupleSchema = "";
mutable map<rstring, rstring> attributeInfo = {};
					
get_tuple_schema_and_attribute_info(myTuple1, tupleSchema,
   attributeInfo, error, $EVAL_PREDICATE_TRACING);

if(error == 0) {
   printStringLn("Get tuple schema function returned successfully. " +
      ", tupleSchema = " + tupleSchema + 
      ", attributeInfo = " + (rstring)attributeInfo);
   } else {
      printStringLn("Get tuple schema function returned an error. Error=" + (rstring)error);
   }

// Following is the usage description for the get_tuple_schema_and_attribute_info function.
// Arg1: Your tuple
// Arg2: A mutable variable of rstring type in which the
//       tuple schema literal string will be returned.
// Arg3: A mutable variable of map<string, rstring> type in which the
//       tuple attribute information will be returned. Map keys will
//       carry the fully qualified tuple attribute names and the
//       map values will carry the SPL type name of the attributes.
// Arg4: A mutable int32 variable to receive non-zero error code if any.
// Arg5: A boolean value to enable debug tracing inside this function.
// It is a void function that returns nothing.
```

## Design considerations
This toolkit came into existence for a specific need with which a large enterprise customer approached the author of this toolkit. There is already a built-in function named *evalPredicate* that is available in the official IBM Streams product. However, that function has certain limitations. To fill that gap, this toolkit with its own **eval_predicate** function is being made available freely via the publicly accessible IBMStreams GitHub. The **eval_predicate** function from this toolkit differs from the *evalPredicate* built-in function in the IBM Streams product in the following ways.

1. This new eval_predicate function allows the user defined rule expression to access map based tuple attributes.

2. This new eval_predicate function allows the user defined rule expression to access nested tuple attributes.

3. This new eval_predicate function allows the user defined rule expression to have operation verbs such as contains, startsWith, endsWith, notContains, notStartsWith, notEndsWith, in, containsCI, startsWithCI, endsWithCI, notContainsCI, notStartsWithCI, notEndsWithCI, inCI, equalsCI, notEqualsCI, sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT, sizeGE.

4. This new eval_predicate function supports the following operations inside the rule.

   a. It supports these relational operations: ==, !=, <, <=, >, >=

   b. It supports these logical operations: ||, &&

   c. It supports these arithmetic operations: +, -, *, /, %

   d. It supports these special operations for rstring, set, list and map: contains, startsWith, endsWith, notContains, notStartsWith, notEndsWith, in, containsCI, startsWithCI, endsWithCI, notContainsCI, notStartsWithCI, notEndsWithCI, inCI, equalsCI, notEqualsCI, sizeEQ, sizeNE, sizeLT, sizeLE, sizeGT, sizeGE

   e. No bitwise operations are supported at this time.

5. It allows a rule expression that can refer to boolean, integer, float, string, set, list, map and other nested tuple attributes. In addition to having many built-in data types inside a collection, it allows list of TUPLE by having user defined custom tuple data types inside a list.

6. It allows certain level of subexpression chaining i.e. nesting in a rule.

   a. It supports zero or single-level or multi-level parenthesis in a given rule.

   b. Via multi-level parenthesis, it allows for evaluating nested rules.

   c. Within a given subexpression, one must use the same logical operators.

   d. Zero parenthesis is used in this example rule:
   
   a == 'hi' && b contains 'xyz' && g[4] > 6.7 && id % 8 == 3

   e. Single-level parenthesis is used within each subexpression in the following example rules:
   
   (a == 'hi') && (b contains 'xyz' || g[4] > 6.7 || id % 8 == 3)

   (a == "hi") && (b contains "xyz") && (g[4] > 6.7) && (id % 8 == 3)

   f. Multi-level parenthesis is used in the following example rules that are made of single-level nested subexpressions:
   
   (a == "hi") && ((b contains "xyz" || g[4] > 6.7) && id % 8 == 3)

   (a == "hi") && (b contains "xyz" && (g[4] > 6.7 || id % 8 == 3))

   (a == "hi") && ((b contains "xyz") || (g[4] > 6.7) || (id % 8 == 3))

   (a == "hi") && ((b contains "xyz") || (g[4] > 6.7 || id % 8 == 3))

   ((a == "hi" || c endsWith 'pqr') && (b contains "xyz")) || (g[4] > 6.7 || id % 8 == 3)

   (((a == 'hi') || (x <= 5) || (t == 3.14) || (p > 7)) && ((j == 3) && (y < 1) && (r == 9)) && s endsWith 'Nation')

   g. Multi-level nested subexpressions are used in the following example rules to make them deeply nested.
   
   ((a.transport.plane.airliner equalsCI 'bOeInG') && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari')) && (((testId equalsCI 'Happy Path') && (a.rack.hw.vendor equalsCI 'Intel')) || (((a.transport.plane.airliner equalsCI 'bOeInG') || (testId equalsCI 'Happy Path')) && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari')))

   (a.transport.plane.airliner equalsCI 'bOeInG' && a.transport.cars.autoMaker equalsCI 'Enzo Ferrari') && ((testId equalsCI 'Happy Path' && a.rack.hw.vendor equalsCI 'Intel') || ((a.transport.plane.airliner equalsCI 'bOeInG2' || testId equalsCI 'Happy Path') && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari') && (a.transport.plane.airliner equalsCI 'bOeInG')) || (testId equalsCI 'Happy Path' && testId equalsCI 'Happy Path') || (testId equalsCI 'Happy Path7' && testId equalsCI 'Happy Path')) && (testId equalsCI 'Happy Path')

   (a.transport.plane.airliner equalsCI 'bOeInG' && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari' || (testId equalsCI 'Happy Path' || a.rack.hw.vendor equalsCI 'Intel2'))) && ((testId equalsCI 'Happy Path' && a.rack.hw.vendor equalsCI 'Intel') || ((a.transport.plane.airliner equalsCI 'bOeInG2' || testId equalsCI 'Happy Path') && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari') && (a.transport.plane.airliner equalsCI 'bOeInG')) || (testId equalsCI 'Happy Path' && testId equalsCI 'Happy Path') || (testId equalsCI 'Happy Path7' && testId equalsCI 'Happy Path')) && (testId equalsCI 'Happy Path')
   
   (a.transport.plane.airliner equalsCI 'bOeInG') && (a.transport.cars.autoMaker equalsCI 'Enzo Ferrari') && ((testId equalsCI 'Happy Path') && ((a.rack.hw.vendor equalsCI 'Intel') || ((a.transport.cars.autoMaker equalsCI 'Enzo Ferrari') && (a.transport.plane.airliner equalsCI 'bOeInG'))))
   
   (Value.Status.Event equalsCI 'PATCHING') && ((Value.Status.UserName containsCI 'EWR') || (Value.Status.EntityState equalsCI 'ENGINEERING') || ((Value.Properties.OwnedBy containsCI 'FER') && ((Value.Status.Availability equalsCI 'Up') || ((Value.Status.Availability equalsCI 'Down') && (Value.Status.StatusCategory3 containsCI 'StatusCategory3'))))) && (Value.Status.StatusCategory4 equalsCI 'StatusCategory4')

   ((city notContainsCI 'Boston') && (city notContainsCI 'Chicago') && (city notContainsCI 'Miami') && (city notContainsCI 'Denver') && (((city equalsCI 'New York') && ((city containsCI 'New York') || (city containsCI 'New') || (city containsCI 'York'))) || ((city containsCI 'ork') || (city containsCI 'W Y') || (city containsCI 'ew') || (city containsCI 'New') || (city containsCI 'York') || (  city containsCI 'ew Yo'))))

   ((city notContainsCI 'Boston') && (city notContainsCI 'Chicago') && (city notContainsCI 'Miami') && (city notContainsCI 'Denver')) && (((city equalsCI 'New York') && ((city containsCI 'New York') || (city containsCI 'New') || (city containsCI 'York'))) || ((city containsCI 'ork') || (city containsCI 'W Y') || (city containsCI 'ew') || (city containsCI 'New') || (city containsCI 'York') || (  city containsCI 'ew Yo')))

## Source code
The complete C++ logic for the **eval_predicate** function is available in the [eval_predicate.h](com.ibm.streamsx.eval_predicate/impl/include/eval_predicate.h) file of this repository.

## Example applications
There is a well documented and well tested example application available in the [EvalPredicateExample.spl](samples/01_eval_predicate_example/com.ibm.streamsx.eval_predicate.test/EvalPredicateExample.spl) file of this repository. Users can browse that example code, compile and run it to become familiar with the usage of the **eval_predicate** function. There is also [FunctionalTests.spl](samples/02_eval_predicate_functional_tests/com.ibm.streamsx.eval_predicate.test/FunctionalTests.spl) which is a comprehensive application that tests the major code paths in the **eval_predicate** function.

A *Makefile* can be found in each of the example applications mentioned above. One can type `make` from a Linux terminal window by being inside the specific example directory. Alternatively, the extracted toolkit directory and the individual example directories can also be imported into IBM Streams Studio or Microsoft Visual Studio Code. Before importing, it is a must to rename that Makefile in that example directory to *Makefile.org*. Only with that renaming of the Makefile, the example applications will build correctly after importing the toolkit into the Studio development environment.

## Getting an official version of this toolkit
One can clone this repository as needed for making code changes. But, for users who only want to use this toolkit in their applications, it is better to download an official version of this toolkit that has a release tag. In the right hand side column of this web page, you will see the *Releases* section. There, you can click on the *Latest* button and then download the tar.gz file which can be extracted for ready use as a dependency in your IBM Streams application project.

## WHATS NEW

see: [CHANGELOG.md](com.ibm.streamsx.eval_predicate/CHANGELOG.md)
