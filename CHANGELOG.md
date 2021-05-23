# Changes

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
