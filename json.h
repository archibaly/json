#ifndef _JSON_H_
#define _JSON_H_

#ifdef __cplusplus
extern "C" {
#endif

/* JSON Types: */
#define JSON_FALSE				0
#define JSON_TRUE				1
#define JSON_NULL				2
#define JSON_NUMBER				3
#define JSON_STRING				4
#define JSON_ARRAY				5
#define JSON_OBJECT				6

#define JSON_IS_REFERENCE		256
#define JSON_STRING_IS_CONST	512

/* the json_t structure: */
typedef struct json {
	struct json *next, *prev;	/* next/prev allow you to walk array/object chains. alternatively, use get_array_size/get_array_item/get_object_item */
	struct json *child;			/* an array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* the type of the item, as above. */

	char *valuestring;			/* the item's string, if type==json_string */
	int valueint;				/* the item's number, if type==json_number */
	double valuedouble;			/* the item's number, if type==json_number */

	char *string;				/* the item's name string, if this item is the child of, or is in the list of subitems of an object. */
} json_t;


/* supply a block of json, and this returns a json_t object you can interrogate. call json_delete when finished. */
extern json_t *json_parse(const char *value);
/* render a json_t entity to text for transfer/storage. free the char* when finished. */
extern char *json_print(json_t *item);
/* render a json_t entity to text for transfer/storage without any formatting. free the char* when finished. */
extern char *json_print_unformatted(json_t *item);
/* render a json_t entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
extern char *json_print_buffered(json_t *item, int prebuffer, int fmt);
/* delete a json_t entity and all subentities. */
extern void json_delete(json_t *c);

/* returns the number of items in an array (or object). */
extern int json_get_array_size(json_t *array);
/* retrieve item number "item" from array "array". returns null if unsuccessful. */
extern json_t *json_get_array_item(json_t *array, int item);
/* get item "string" from object. case insensitive. */
extern json_t *json_get_object_item(json_t *object, const char *string);

/* for analysing failed parses. this returns a pointer to the parse error. you'll probably need to look a few chars back to make sense of it. defined when json_parse() returns 0. 0 when json_parse() succeeds. */
extern const char *json_get_error_ptr(void);

/* these calls create a json_t item of the appropriate type. */
extern json_t *json_create_null(void);
extern json_t *json_create_true(void);
extern json_t *json_create_false(void);
extern json_t *json_create_bool(int b);
extern json_t *json_create_number(double num);
extern json_t *json_create_string(const char *string);
extern json_t *json_create_array(void);
extern json_t *json_create_object(void);

/* these utilities create an array of count items. */
extern json_t *json_create_int_array(const int *numbers, int count);
extern json_t *json_create_float_array(const float *numbers, int count);
extern json_t *json_create_double_array(const double *numbers, int count);
extern json_t *json_create_string_array(const char **strings, int count);

/* append item to the specified array/object. */
extern void json_add_item_to_array(json_t *array, json_t *item);
extern void json_add_item_to_object(json_t *object, const char *string,
								  json_t *item);
extern void json_add_item_to_objectcs(json_t *object, const char *string, json_t *item);	/* use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the json_t object */
/* append reference to item to the specified array/object. use this when you want to add an existing json_t to a new json_t, but don't want to corrupt your existing json_t. */
extern void json_add_item_reference_to_array(json_t *array, json_t *item);
extern void json_add_item_reference_to_object(json_t *object,
										   const char *string,
										   json_t *item);

/* remove/detatch items from arrays/objects. */
extern json_t *json_detach_item_from_array(json_t *array, int which);
extern void json_delete_item_from_array(json_t *array, int which);
extern json_t *json_detach_item_from_object(json_t *object,
										 const char *string);
extern void json_delete_item_from_object(json_t *object, const char *string);

/* update array items. */
extern void json_insert_item_in_array(json_t *array, int which, json_t *newitem);	/* shifts pre-existing items to the right. */
extern void json_replace_item_in_array(json_t *array, int which,
									 json_t *newitem);
extern void json_replace_item_ino_bject(json_t *object, const char *string,
									  json_t *newitem);

/* duplicate a json_t item */
extern json_t *json_duplicate(json_t *item, int recurse);
/* duplicate will create a new, identical json_t item to the one you pass, in new memory that will
need to be released. with recurse!=0, it will duplicate any children connected to the item.
the item->next and ->prev pointers are always zero on return from duplicate. */

/* parsewithopts allows you to require (and check) that the json is null terminated, and to retrieve the pointer to the final byte parsed. */
extern json_t *json_parse_with_opts(const char *value,
								  const char **return_parse_end,
								  int require_null_terminated);

extern void json_minify(char *json);

/* macros for creating things quickly. */
#define json_add_null_to_object(object,name)		json_add_item_to_object(object, name, json_create_null())
#define json_add_true_to_object(object,name)		json_add_item_to_object(object, name, json_create_true())
#define json_add_false_to_object(object,name)		json_add_item_to_object(object, name, json_create_false())
#define json_add_bool_to_object(object,name,b)		json_add_item_to_object(object, name, json_create_bool(b))
#define json_add_number_to_object(object,name,n)	json_add_item_to_object(object, name, json_create_number(n))
#define json_add_string_to_object(object,name,s)	json_add_item_to_object(object, name, json_create_string(s))

/* when assigning an integer value, it needs to be propagated to valuedouble too. */
#define json_set_int_value(object,val)				((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define json_set_number_value(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif
#endif	/* _JSON_H_ */
