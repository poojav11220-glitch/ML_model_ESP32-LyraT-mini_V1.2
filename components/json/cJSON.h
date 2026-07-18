/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef cJSON__h
#define cJSON__h

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False   (1 << 0)
#define cJSON_True    (1 << 1)
#define cJSON_NULL    (1 << 2)
#define cJSON_Number  (1 << 3)
#define cJSON_String  (1 << 4)
#define cJSON_Array   (1 << 5)
#define cJSON_Object  (1 << 6)
#define cJSON_Raw     (1 << 7) /* raw json */

#define cJSON_IsReference    256
#define cJSON_StringIsConst  512

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;  /* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
    double valuedouble;
    char *string;
} cJSON;

typedef struct cJSON_Hooks {
    void *(* malloc_fn)(size_t sz);
    void (* free_fn)(void *ptr);
} cJSON_Hooks;

typedef int cJSON_bool;

#define cJSON_SetNumberValue(object, number) ((object != NULL) ? (object)->valuedouble = (object)->valueint = (number) : (number))
#define cJSON_SetValuestring(object, valuestring) ((object != NULL) ? cJSON_free((object)->valuestring), (object)->valuestring = valuestring : NULL)
#define cJSON_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)
#define cJSON_malloc(size) cJSON_GetAllocationFunctions().malloc_fn(size)
#define cJSON_free(object) cJSON_GetAllocationFunctions().free_fn((object))

extern void cJSON_InitHooks(cJSON_Hooks* hooks);
extern cJSON_Hooks cJSON_GetAllocationFunctions(void);

extern cJSON *cJSON_Parse(const char *value);
extern cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length);
extern cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated);
extern cJSON *cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated);
extern char  *cJSON_Print(const cJSON *item);
extern char  *cJSON_PrintUnformatted(const cJSON *item);
extern char  *cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt);
extern cJSON_bool cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format);
extern void   cJSON_Delete(cJSON *item);

extern int    cJSON_GetArraySize(const cJSON *array);
extern cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
extern cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string);
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
extern cJSON_bool cJSON_HasObjectItem(const cJSON *object, const char *string);
extern const char *cJSON_GetErrorPtr(void);

extern char *cJSON_GetStringValue(const cJSON * const item);
extern double cJSON_GetNumberValue(const cJSON * const item);

extern cJSON_bool cJSON_IsInvalid(const cJSON * const item);
extern cJSON_bool cJSON_IsFalse(const cJSON * const item);
extern cJSON_bool cJSON_IsTrue(const cJSON * const item);
extern cJSON_bool cJSON_IsBool(const cJSON * const item);
extern cJSON_bool cJSON_IsNull(const cJSON * const item);
extern cJSON_bool cJSON_IsNumber(const cJSON * const item);
extern cJSON_bool cJSON_IsString(const cJSON * const item);
extern cJSON_bool cJSON_IsArray(const cJSON * const item);
extern cJSON_bool cJSON_IsObject(const cJSON * const item);
extern cJSON_bool cJSON_IsRaw(const cJSON * const item);

extern cJSON *cJSON_CreateNull(void);
extern cJSON *cJSON_CreateTrue(void);
extern cJSON *cJSON_CreateFalse(void);
extern cJSON *cJSON_CreateBool(cJSON_bool boolean);
extern cJSON *cJSON_CreateNumber(double num);
extern cJSON *cJSON_CreateString(const char *string);
extern cJSON *cJSON_CreateRaw(const char *raw);
extern cJSON *cJSON_CreateArray(void);
extern cJSON *cJSON_CreateObject(void);
extern cJSON *cJSON_CreateStringReference(const char *string);
extern cJSON *cJSON_CreateObjectReference(const cJSON *child);
extern cJSON *cJSON_CreateArrayReference(const cJSON *child);
extern cJSON *cJSON_CreateIntArray(const int *numbers, int count);
extern cJSON *cJSON_CreateFloatArray(const float *numbers, int count);
extern cJSON *cJSON_CreateDoubleArray(const double *numbers, int count);
extern cJSON *cJSON_CreateStringArray(const char **strings, int count);

extern cJSON_bool cJSON_AddItemToArray(cJSON *array, cJSON *item);
extern cJSON_bool cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
extern cJSON_bool cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item);
extern cJSON_bool cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
extern cJSON_bool cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);

extern cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item);
extern cJSON *cJSON_DetachItemFromArray(cJSON *array, int which);
extern void   cJSON_DeleteItemFromArray(cJSON *array, int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string);
extern cJSON *cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string);
extern void   cJSON_DeleteItemFromObject(cJSON *object, const char *string);
extern void   cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

extern cJSON_bool cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem);
extern cJSON_bool cJSON_ReplaceItemViaPointer(cJSON *const parent, cJSON *const item, cJSON *replacement);
extern cJSON_bool cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem);
extern cJSON_bool cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem);
extern cJSON_bool cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem);

extern cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
extern cJSON_bool cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive);
extern void cJSON_Minify(char *json);

extern cJSON *cJSON_AddNullToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddTrueToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddFalseToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean);
extern cJSON *cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number);
extern cJSON *cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string);
extern cJSON *cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw);
extern cJSON *cJSON_AddObjectToObject(cJSON * const object, const char * const name);
extern cJSON *cJSON_AddArrayToObject(cJSON * const object, const char * const name);

#ifdef __cplusplus
}
#endif

#endif
