//--------------------------------------------------------------------------------------------------
/**
 * @page c_jsonString JSON String Parser API
 *
 * @ref jsonString.h "API Reference" <br>
 *
 * <hr>
 *
 * This API is intended to provide parsing of a JSON string for C-based programs.  It allows you
 * to extract specific JSON fields from a JSON string according to a JavaScript-style
 * extraction specification.  The extracted string can then be converted to other types, such as
 * bool or double.
 *
 * json_Extract() can be used to extract a subset of a JSON string into another string.
 *
 * The extraction specifiers look like "x" or "x.y" or "[3]" or "x[3].y", etc.
 *
 * json_Extract() will tell you the data type of the thing that was extracted.  The following
 * functions can then be used to convert Boolean value strings or numbers into cardinal C data
 * types:
 *  - json_ConvertToBoolean()
 *  - json_ConvertToNumber()
 *
 * In addition, json_IsValid() is provided for validating JSON.
 *
 *
 *  @section c_jsonString_threads Multi-Threading
 *
 * This API is thread safe.
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

/** @file json.h
 *
 * @ref c_jsonString include file.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef JSON_H_INCLUDE_GUARD
#define JSON_H_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Enumeration of all the different data types supported by JSON.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    JSON_TYPE_NULL,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_NUMBER,
    JSON_TYPE_STRING,
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
}
json_DataType_t;


//--------------------------------------------------------------------------------------------------
/**
 * Extract an object member or array element from a JSON data value, based on a given
 * extraction specifier.
 *
 * The extraction specifiers look like "x" or "x.y" or "[3]" or "x[3].y", etc.
 *
 * @return
 *  - LE_OK if successful
 *  - LE_FORMAT_ERROR if there's something wrong with the input JSON string.
 *  - LE_BAD_PARAMETER if there's something wrong with the extraction specification.
 *  - LE_NOT_FOUND if the thing we are trying to extract doesn't exist in the JSON input.
 *  - LE_OVERFLOW if the provided result buffer isn't big enough.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t json_Extract
(
    char* resultBuffPtr,    ///< [OUT] Ptr to where to put the extracted JSON.
    size_t resultBuffSize,  ///< [IN] Size of the result buffer, in bytes, including space for null.
    const char* jsonValue,   ///< [IN] Original JSON string to extract from.
    const char* extractionSpec, ///< [IN] the extraction specification.
    json_DataType_t* dataTypePtr  ///< [OUT] Ptr to where to put the data type of extracted JSON
);


//--------------------------------------------------------------------------------------------------
/**
 * Convert a JSON value into a Boolean value.
 *
 * @return The Boolean value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool json_ConvertToBoolean
(
    const char* jsonValue
);


//--------------------------------------------------------------------------------------------------
/**
 * Convert a JSON value into a numeric value.
 *
 * @return The numeric value.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED double json_ConvertToNumber
(
    const char* jsonValue
);


//--------------------------------------------------------------------------------------------------
/**
 * Validate a JSON string.
 *
 * @return true if the string is valid JSON.  false if not.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool json_IsValid
(
    const char* jsonValue
);


//--------------------------------------------------------------------------------------------------
/**
 * Get a printable string containing the name of a given data type.
 *
 * @return Pointer to the null-terminated string.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED const char* json_GetDataTypeName
(
    json_DataType_t dataType
);



#endif // JSON_H_INCLUDE_GUARD
