#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "json.h"

static const char *ep;

const char *json_get_error_ptr(void)
{
	return ep;
}

/* internal constructor. */
static json_t *json_new_item(void)
{
	json_t *node = (json_t *) malloc(sizeof(json_t));
	if (node)
		memset(node, 0, sizeof(json_t));
	return node;
}

void json_delete(json_t *c)
{
	json_t *next;
	while (c) {
		next = c->next;
		if (!(c->type & JSON_IS_REFERENCE) && c->child)
			json_delete(c->child);
		if (!(c->type & JSON_IS_REFERENCE) && c->valuestring)
			free(c->valuestring);
		if (!(c->type & JSON_STRING_IS_CONST) && c->string)
			free(c->string);
		free(c);
		c = next;
	}
}

/* parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(json_t *item, const char *num)
{
	double n = 0, sign = 1, scale = 0;
	int subscale = 0, signsubscale = 1;

	if (*num == '-')
		sign = -1, num++;		/* Has sign? */
	if (*num == '0')
		num++;					/* is zero */
	if (*num >= '1' && *num <= '9')
		do
			n = (n * 10.0) + (*num++ - '0');
		while (*num >= '0' && *num <= '9');	/* Number? */
	if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
		num++;
		do
			n = (n * 10.0) + (*num++ - '0'), scale--;
		while (*num >= '0' && *num <= '9');
	}							/* Fractional part? */
	if (*num == 'e' || *num == 'E') {	/* Exponent? */
		num++;
		if (*num == '+')
			num++;
		else if (*num == '-')
			signsubscale = -1, num++;	/* With sign? */
		while (*num >= '0' && *num <= '9')
			subscale = (subscale * 10) + (*num++ - '0');	/* Number? */
	}

	n = sign * n * pow(10.0, (scale + subscale * signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */

	item->valuedouble = n;
	item->valueint = (int)n;
	item->type = JSON_NUMBER;
	return num;
}

static int pow2gt(int x)
{
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

typedef struct {
	char *buffer;
	int length;
	int offset;
} printbuffer;

static char *ensure(printbuffer * p, int needed)
{
	char *newbuffer;
	int newsize;
	if (!p || !p->buffer)
		return 0;
	needed += p->offset;
	if (needed <= p->length)
		return p->buffer + p->offset;

	newsize = pow2gt(needed);
	newbuffer = (char *)malloc(newsize);
	if (!newbuffer) {
		free(p->buffer);
		p->length = 0, p->buffer = 0;
		return 0;
	}
	if (newbuffer)
		memcpy(newbuffer, p->buffer, p->length);
	free(p->buffer);
	p->length = newsize;
	p->buffer = newbuffer;
	return newbuffer + p->offset;
}

static int update(printbuffer * p)
{
	char *str;
	if (!p || !p->buffer)
		return 0;
	str = p->buffer + p->offset;
	return p->offset + strlen(str);
}

/* render the number nicely from the given item into a string. */
static char *print_number(json_t *item, printbuffer * p)
{
	char *str = 0;
	double d = item->valuedouble;
	if (d == 0) {
		if (p)
			str = ensure(p, 2);
		else
			str = (char *)malloc(2);	/* special case for 0. */
		if (str)
			strcpy(str, "0");
	} else if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX
			   && d >= INT_MIN) {
		if (p)
			str = ensure(p, 21);
		else
			str = (char *)malloc(21);	/* 2^64+1 can be represented in 21 chars. */
		if (str)
			sprintf(str, "%d", item->valueint);
	} else {
		if (p)
			str = ensure(p, 64);
		else
			str = (char *)malloc(64);	/* This is a nice tradeoff. */
		if (str) {
			if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60)
				sprintf(str, "%.0f", d);
			else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
				sprintf(str, "%e", d);
			else
				sprintf(str, "%f", d);
		}
	}
	return str;
}

static unsigned parse_hex4(const char *str)
{
	unsigned h = 0;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;
	h = h << 4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;
	h = h << 4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;
	h = h << 4;
	str++;
	if (*str >= '0' && *str <= '9')
		h += (*str) - '0';
	else if (*str >= 'A' && *str <= 'F')
		h += 10 + (*str) - 'A';
	else if (*str >= 'a' && *str <= 'f')
		h += 10 + (*str) - 'a';
	else
		return 0;
	return h;
}

/* parse the input text into an unescaped cstring, and populate item. */
static const unsigned char first_byte_mark[7] =
	{ 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(json_t *item, const char *str)
{
	const char *ptr = str + 1;
	char *ptr2;
	char *out;
	int len = 0;
	unsigned uc, uc2;
	if (*str != '\"') {
		ep = str;
		return 0;
	}
	/* not a string! */
	while (*ptr != '\"' && *ptr && ++len)
		if (*ptr++ == '\\')
			ptr++;				/* Skip escaped quotes. */

	out = (char *)malloc(len + 1);	/* This is how long we need for the string, roughly. */
	if (!out)
		return 0;

	ptr = str + 1;
	ptr2 = out;
	while (*ptr != '\"' && *ptr) {
		if (*ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u':			/* transcode utf16 to utf8. */
				uc = parse_hex4(ptr + 1);
				ptr += 4;		/* get the unicode char. */

				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
					break;		/* check for invalid.   */

				if (uc >= 0xD800 && uc <= 0xDBFF) {	/* UTF16 surrogate pairs. */
					if (ptr[1] != '\\' || ptr[2] != 'u')
						break;	/* missing second-half of surrogate.    */
					uc2 = parse_hex4(ptr + 3);
					ptr += 6;
					if (uc2 < 0xDC00 || uc2 > 0xDFFF)
						break;	/* invalid second-half of surrogate.    */
					uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
				}

				len = 4;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				else if (uc < 0x10000)
					len = 3;
				ptr2 += len;

				switch (len) {
				case 4:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 3:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 1:
					*--ptr2 = (uc | first_byte_mark[len]);
				}
				ptr2 += len;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"')
		ptr++;
	item->valuestring = out;
	item->type = JSON_STRING;
	return ptr;
}

/* render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str, printbuffer * p)
{
	const char *ptr;
	char *ptr2, *out;
	int len = 0, flag = 0;
	unsigned char token;

	for (ptr = str; *ptr; ptr++)
		flag |= ((*ptr > 0 && *ptr < 32) || (*ptr == '\"')
				 || (*ptr == '\\')) ? 1 : 0;
	if (!flag) {
		len = ptr - str;
		if (p)
			out = ensure(p, len + 3);
		else
			out = (char *)malloc(len + 3);
		if (!out)
			return 0;
		ptr2 = out;
		*ptr2++ = '\"';
		strcpy(ptr2, str);
		ptr2[len] = '\"';
		ptr2[len + 1] = 0;
		return out;
	}

	if (!str) {
		if (p)
			out = ensure(p, 3);
		else
			out = (char *)malloc(3);
		if (!out)
			return 0;
		strcpy(out, "\"\"");
		return out;
	}
	ptr = str;
	while ((token = *ptr) && ++len) {
		if (strchr("\"\\\b\f\n\r\t", token))
			len++;
		else if (token < 32)
			len += 5;
		ptr++;
	}

	if (p)
		out = ensure(p, len + 3);
	else
		out = (char *)malloc(len + 3);
	if (!out)
		return 0;

	ptr2 = out;
	ptr = str;
	*ptr2++ = '\"';
	while (*ptr) {
		if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			*ptr2++ = '\\';
			switch (token = *ptr++) {
			case '\\':
				*ptr2++ = '\\';
				break;
			case '\"':
				*ptr2++ = '\"';
				break;
			case '\b':
				*ptr2++ = 'b';
				break;
			case '\f':
				*ptr2++ = 'f';
				break;
			case '\n':
				*ptr2++ = 'n';
				break;
			case '\r':
				*ptr2++ = 'r';
				break;
			case '\t':
				*ptr2++ = 't';
				break;
			default:
				sprintf(ptr2, "u%04x", token);
				ptr2 += 5;
				break;			/* escape and print */
			}
		}
	}
	*ptr2++ = '\"';
	*ptr2++ = 0;
	return out;
}

/* invote print_string_ptr (which is useful) on an item. */
static char *print_string(json_t *item, printbuffer * p)
{
	return print_string_ptr(item->valuestring, p);
}

/* predeclare these prototypes. */
static const char *parse_value(json_t *item, const char *value);
static char *print_value(json_t *item, int depth, int fmt, printbuffer *p);
static const char *parse_array(json_t *item, const char *value);
static char *print_array(json_t *item, int depth, int fmt, printbuffer *p);
static const char *parse_object(json_t *item, const char *value);
static char *print_object(json_t *item, int depth, int fmt, printbuffer *p);

/* utility to jump whitespace and cr/lf */
static const char *skip(const char *in)
{
	while (in && *in && (unsigned char)*in <= 32)
		in++;
	return in;
}

/* parse an object - create a new root, and populate. */
json_t *json_parse_with_opts(const char *value, const char **return_parse_end,
						   int require_null_terminated)
{
	const char *end = 0;
	json_t *c = json_new_item();
	ep = 0;
	if (!c)
		return 0;				/* memory fail */

	end = parse_value(c, skip(value));
	if (!end) {
		json_delete(c);
		return 0;
	}

	/* parse failure. ep is set. */
	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {
		end = skip(end);
		if (*end) {
			json_delete(c);
			ep = end;
			return 0;
		}
	}
	if (return_parse_end)
		*return_parse_end = end;
	return c;
}

/* default options for json_parse */
json_t *json_parse(const char *value)
{
	return json_parse_with_opts(value, 0, 0);
}

/* render a json_t item/entity/structure to text. */
char *json_print(json_t *item)
{
	return print_value(item, 0, 1, 0);
}

char *json_print_unformatted(json_t *item)
{
	return print_value(item, 0, 0, 0);
}

char *json_print_buffered(json_t *item, int prebuffer, int fmt)
{
	printbuffer p;
	p.buffer = (char *)malloc(prebuffer);
	p.length = prebuffer;
	p.offset = 0;
	return print_value(item, 0, fmt, &p);
	return p.buffer;
}

/* parser core - when encountering text, process appropriately. */
static const char *parse_value(json_t *item, const char *value)
{
	if (!value)
		return 0;				/* Fail on null. */
	if (!strncmp(value, "null", 4)) {
		item->type = JSON_NULL;
		return value + 4;
	}
	if (!strncmp(value, "false", 5)) {
		item->type = JSON_FALSE;
		return value + 5;
	}
	if (!strncmp(value, "true", 4)) {
		item->type = JSON_TRUE;
		item->valueint = 1;
		return value + 4;
	}
	if (*value == '\"') {
		return parse_string(item, value);
	}
	if (*value == '-' || (*value >= '0' && *value <= '9')) {
		return parse_number(item, value);
	}
	if (*value == '[') {
		return parse_array(item, value);
	}
	if (*value == '{') {
		return parse_object(item, value);
	}

	ep = value;
	return 0;					/* failure. */
}

/* render a value to text. */
static char *print_value(json_t *item, int depth, int fmt, printbuffer * p)
{
	char *out = 0;
	if (!item)
		return 0;
	if (p) {
		switch ((item->type) & 255) {
		case JSON_NULL:
			out = ensure(p, 5);
			if (out)
				strcpy(out, "null");
			break;
		case JSON_FALSE:
			out = ensure(p, 6);
			if (out)
				strcpy(out, "false");
			break;
		case JSON_TRUE:
			out = ensure(p, 5);
			if (out)
				strcpy(out, "true");
			break;
		case JSON_NUMBER:
			out = print_number(item, p);
			break;
		case JSON_STRING:
			out = print_string(item, p);
			break;
		case JSON_ARRAY:
			out = print_array(item, depth, fmt, p);
			break;
		case JSON_OBJECT:
			out = print_object(item, depth, fmt, p);
			break;
		}
	} else {
		switch ((item->type) & 255) {
			case JSON_NULL:
				out = strdup("null");
				break;
			case JSON_FALSE:
				out = strdup("false");
				break;
			case JSON_TRUE:
				out = strdup("true");
				break;
			case JSON_NUMBER:
				out = print_number(item, 0);
				break;
			case JSON_STRING:
				out = print_string(item, 0);
				break;
			case JSON_ARRAY:
				out = print_array(item, depth, fmt, 0);
				break;
			case JSON_OBJECT:
				out = print_object(item, depth, fmt, 0);
				break;
		}
	}
	return out;
}

/* build an array from input text. */
static const char *parse_array(json_t *item, const char *value)
{
	json_t *child;
	if (*value != '[') {
		ep = value;
		return 0;
	}
	/* not an array! */
	item->type = JSON_ARRAY;
	value = skip(value + 1);
	if (*value == ']')
		return value + 1;		/* empty array. */

	item->child = child = json_new_item();
	if (!item->child)
		return 0;				/* memory fail */
	value = skip(parse_value(child, skip(value)));	/* skip any spacing, get the value. */
	if (!value)
		return 0;

	while (*value == ',') {
		json_t *new_item;
		if (!(new_item = json_new_item()))
			return 0;			/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_value(child, skip(value + 1)));
		if (!value)
			return 0;			/* memory fail */
	}

	if (*value == ']')
		return value + 1;		/* end of array */
	ep = value;
	return 0;					/* malformed. */
}

/* render an array to text */
static char *print_array(json_t *item, int depth, int fmt, printbuffer * p)
{
	char **entries;
	char *out = 0, *ptr, *ret;
	int len = 5;
	json_t *child = item->child;
	int numentries = 0, i = 0, fail = 0;
	size_t tmplen = 0;

	/* how many entries in the array? */
	while (child)
		numentries++, child = child->next;
	/* explicitly handle numentries==0 */
	if (!numentries) {
		if (p)
			out = ensure(p, 3);
		else
			out = (char *)malloc(3);
		if (out)
			strcpy(out, "[]");
		return out;
	}

	if (p) {
		/* compose the output array. */
		i = p->offset;
		ptr = ensure(p, 1);
		if (!ptr)
			return 0;
		*ptr = '[';
		p->offset++;
		child = item->child;
		while (child && !fail) {
			print_value(child, depth + 1, fmt, p);
			p->offset = update(p);
			if (child->next) {
				len = fmt ? 2 : 1;
				ptr = ensure(p, len + 1);
				if (!ptr)
					return 0;
				*ptr++ = ',';
				if (fmt)
					*ptr++ = ' ';
				*ptr = 0;
				p->offset += len;
			}
			child = child->next;
		}
		ptr = ensure(p, 2);
		if (!ptr)
			return 0;
		*ptr++ = ']';
		*ptr = 0;
		out = (p->buffer) + i;
	} else {
		/* allocate an array to hold the values for each */
		entries = (char **)malloc(numentries * sizeof(char *));
		if (!entries)
			return 0;
		memset(entries, 0, numentries * sizeof(char *));
		/* Retrieve all the results: */
		child = item->child;
		while (child && !fail) {
			ret = print_value(child, depth + 1, fmt, 0);
			entries[i++] = ret;
			if (ret)
				len += strlen(ret) + 2 + (fmt ? 1 : 0);
			else
				fail = 1;
			child = child->next;
		}

		/* if we didn't fail, try to malloc the output string */
		if (!fail)
			out = (char *)malloc(len);
		/* if that fails, we fail. */
		if (!out)
			fail = 1;

		/* handle failure. */
		if (fail) {
			for (i = 0; i < numentries; i++)
				if (entries[i])
					free(entries[i]);
			free(entries);
			return 0;
		}

		/* compose the output array. */
		*out = '[';
		ptr = out + 1;
		*ptr = 0;
		for (i = 0; i < numentries; i++) {
			tmplen = strlen(entries[i]);
			memcpy(ptr, entries[i], tmplen);
			ptr += tmplen;
			if (i != numentries - 1) {
				*ptr++ = ',';
				if (fmt)
					*ptr++ = ' ';
				*ptr = 0;
			}
			free(entries[i]);
		}
		free(entries);
		*ptr++ = ']';
		*ptr++ = 0;
	}
	return out;
}

/* build an object from the text. */
static const char *parse_object(json_t *item, const char *value)
{
	json_t *child;
	if (*value != '{') {
		ep = value;
		return 0;
	}
	/* not an object! */
	item->type = JSON_OBJECT;
	value = skip(value + 1);
	if (*value == '}')
		return value + 1;		/* empty array. */

	item->child = child = json_new_item();
	if (!item->child)
		return 0;
	value = skip(parse_string(child, skip(value)));
	if (!value)
		return 0;
	child->string = child->valuestring;
	child->valuestring = 0;
	if (*value != ':') {
		ep = value;
		return 0;
	}							/* fail! */
	value = skip(parse_value(child, skip(value + 1)));	/* skip any spacing, get the value. */
	if (!value)
		return 0;

	while (*value == ',') {
		json_t *new_item;
		if (!(new_item = json_new_item()))
			return 0;			/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		value = skip(parse_string(child, skip(value + 1)));
		if (!value)
			return 0;
		child->string = child->valuestring;
		child->valuestring = 0;
		if (*value != ':') {
			ep = value;
			return 0;
		}						/* fail! */
		value = skip(parse_value(child, skip(value + 1)));	/* skip any spacing, get the value. */
		if (!value)
			return 0;
	}

	if (*value == '}')
		return value + 1;		/* end of array */
	ep = value;
	return 0;					/* malformed. */
}

/* render an object to text. */
static char *print_object(json_t *item, int depth, int fmt, printbuffer * p)
{
	char **entries = 0, **names = 0;
	char *out = 0, *ptr, *ret, *str;
	int len = 7, i = 0, j;
	json_t *child = item->child;
	int numentries = 0, fail = 0;
	size_t tmplen = 0;
	/* count the number of entries. */
	while (child)
		numentries++, child = child->next;
	/* explicitly handle empty object case */
	if (!numentries) {
		if (p)
			out = ensure(p, fmt ? depth + 4 : 3);
		else
			out = (char *)malloc(fmt ? depth + 4 : 3);
		if (!out)
			return 0;
		ptr = out;
		*ptr++ = '{';
		if (fmt) {
			*ptr++ = '\n';
			for (i = 0; i < depth - 1; i++)
				*ptr++ = '\t';
		}
		*ptr++ = '}';
		*ptr++ = 0;
		return out;
	}
	if (p) {
		/* compose the output: */
		i = p->offset;
		len = fmt ? 2 : 1;
		ptr = ensure(p, len + 1);
		if (!ptr)
			return 0;
		*ptr++ = '{';
		if (fmt)
			*ptr++ = '\n';
		*ptr = 0;
		p->offset += len;
		child = item->child;
		depth++;
		while (child) {
			if (fmt) {
				ptr = ensure(p, depth);
				if (!ptr)
					return 0;
				for (j = 0; j < depth; j++)
					*ptr++ = '\t';
				p->offset += depth;
			}
			print_string_ptr(child->string, p);
			p->offset = update(p);

			len = fmt ? 2 : 1;
			ptr = ensure(p, len);
			if (!ptr)
				return 0;
			*ptr++ = ':';
			if (fmt)
				*ptr++ = '\t';
			p->offset += len;

			print_value(child, depth, fmt, p);
			p->offset = update(p);

			len = (fmt ? 1 : 0) + (child->next ? 1 : 0);
			ptr = ensure(p, len + 1);
			if (!ptr)
				return 0;
			if (child->next)
				*ptr++ = ',';
			if (fmt)
				*ptr++ = '\n';
			*ptr = 0;
			p->offset += len;
			child = child->next;
		}
		ptr = ensure(p, fmt ? (depth + 1) : 2);
		if (!ptr)
			return 0;
		if (fmt)
			for (i = 0; i < depth - 1; i++)
				*ptr++ = '\t';
		*ptr++ = '}';
		*ptr = 0;
		out = (p->buffer) + i;
	} else {
		/* hllocate space for the names and the objects */
		entries = (char **)malloc(numentries * sizeof(char *));
		if (!entries)
			return 0;
		names = (char **)malloc(numentries * sizeof(char *));
		if (!names) {
			free(entries);
			return 0;
		}
		memset(entries, 0, sizeof(char *) * numentries);
		memset(names, 0, sizeof(char *) * numentries);

		/* hollect all the results into our arrays: */
		child = item->child;
		depth++;
		if (fmt)
			len += depth;
		while (child) {
			names[i] = str = print_string_ptr(child->string, 0);
			entries[i++] = ret = print_value(child, depth, fmt, 0);
			if (str && ret)
				len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
			else
				fail = 1;
			child = child->next;
		}

		/* try to allocate the output string */
		if (!fail)
			out = (char *)malloc(len);
		if (!out)
			fail = 1;

		/* handle failure */
		if (fail) {
			for (i = 0; i < numentries; i++) {
				if (names[i])
					free(names[i]);
				if (entries[i])
					free(entries[i]);
			}
			free(names);
			free(entries);
			return 0;
		}

		/* compose the output: */
		*out = '{';
		ptr = out + 1;
		if (fmt)
			*ptr++ = '\n';
		*ptr = 0;
		for (i = 0; i < numentries; i++) {
			if (fmt)
				for (j = 0; j < depth; j++)
					*ptr++ = '\t';
			tmplen = strlen(names[i]);
			memcpy(ptr, names[i], tmplen);
			ptr += tmplen;
			*ptr++ = ':';
			if (fmt)
				*ptr++ = '\t';
			strcpy(ptr, entries[i]);
			ptr += strlen(entries[i]);
			if (i != numentries - 1)
				*ptr++ = ',';
			if (fmt)
				*ptr++ = '\n';
			*ptr = 0;
			free(names[i]);
			free(entries[i]);
		}

		free(names);
		free(entries);
		if (fmt)
			for (i = 0; i < depth - 1; i++)
				*ptr++ = '\t';
		*ptr++ = '}';
		*ptr++ = 0;
	}
	return out;
}

/* get array size/item / object item. */
int json_get_array_size(json_t *array)
{
	json_t *c = array->child;
	int i = 0;
	while (c)
		i++, c = c->next;
	return i;
}

json_t *json_get_array_item(json_t *array, int item)
{
	json_t *c = array->child;
	while (c && item > 0)
		item--, c = c->next;
	return c;
}

json_t *json_get_object_item(json_t *object, const char *string)
{
	json_t *c = object->child;
	while (c && strcasecmp(c->string, string))
		c = c->next;
	return c;
}

/* utility for array list handling. */
static void suffix_object(json_t *prev, json_t *item)
{
	prev->next = item;
	item->prev = prev;
}

/* utility for handling references. */
static json_t *create_reference(json_t *item)
{
	json_t *ref = json_new_item();
	if (!ref)
		return 0;
	memcpy(ref, item, sizeof(json_t));
	ref->string = 0;
	ref->type |= JSON_IS_REFERENCE;
	ref->next = ref->prev = 0;
	return ref;
}

/* add item to array/object. */
void json_add_item_to_array(json_t *array, json_t *item)
{
	json_t *c = array->child;
	if (!item)
		return;
	if (!c) {
		array->child = item;
	} else {
		while (c && c->next)
			c = c->next;
		suffix_object(c, item);
	}
}

void json_add_item_to_object(json_t *object, const char *string, json_t *item)
{
	if (!item)
		return;
	if (item->string)
		free(item->string);
	item->string = strdup(string);
	json_add_item_to_array(object, item);
}

void json_add_item_to_objectCS(json_t *object, const char *string, json_t *item)
{
	if (!item)
		return;
	if (!(item->type & JSON_STRING_IS_CONST) && item->string)
		free(item->string);
	item->string = (char *)string;
	item->type |= JSON_STRING_IS_CONST;
	json_add_item_to_array(object, item);
}

void json_add_item_reference_to_array(json_t *array, json_t *item)
{
	json_add_item_to_array(array, create_reference(item));
}

void json_add_item_reference_to_object(json_t *object, const char *string,
		json_t *item)
{
	json_add_item_to_object(object, string, create_reference(item));
}

json_t *json_detach_item_from_array(json_t *array, int which)
{
	json_t *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return 0;
	if (c->prev)
		c->prev->next = c->next;
	if (c->next)
		c->next->prev = c->prev;
	if (c == array->child)
		array->child = c->next;
	c->prev = c->next = 0;
	return c;
}

void json_delete_item_from_array(json_t *array, int which)
{
	json_delete(json_detach_item_from_array(array, which));
}

json_t *json_detach_item_from_object(json_t *object, const char *string)
{
	int i = 0;
	json_t *c = object->child;
	while (c && strcasecmp(c->string, string))
		i++, c = c->next;
	if (c)
		return json_detach_item_from_array(object, i);
	return 0;
}

void json_delete_item_from_object(json_t *object, const char *string)
{
	json_delete(json_detach_item_from_object(object, string));
}

/* replace array/object items with new ones. */
void json_insert_item_in_array(json_t *array, int which, json_t *newitem)
{
	json_t *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c) {
		json_add_item_to_array(array, newitem);
		return;
	}
	newitem->next = c;
	newitem->prev = c->prev;
	c->prev = newitem;
	if (c == array->child)
		array->child = newitem;
	else
		newitem->prev->next = newitem;
}

void json_replace_item_in_array(json_t *array, int which, json_t *newitem)
{
	json_t *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return;
	newitem->next = c->next;
	newitem->prev = c->prev;
	if (newitem->next)
		newitem->next->prev = newitem;
	if (c == array->child)
		array->child = newitem;
	else
		newitem->prev->next = newitem;
	c->next = c->prev = 0;
	json_delete(c);
}

void json_replace_item_in_object(json_t *object, const char *string,
		json_t *newitem)
{
	int i = 0;
	json_t *c = object->child;
	while (c && strcasecmp(c->string, string))
		i++, c = c->next;
	if (c) {
		newitem->string = strdup(string);
		json_replace_item_in_array(object, i, newitem);
	}
}

/* create basic types: */
json_t *json_create_null(void)
{
	json_t *item = json_new_item();
	if (item)
		item->type = JSON_NULL;
	return item;
}

json_t *json_create_true(void)
{
	json_t *item = json_new_item();
	if (item)
		item->type = JSON_TRUE;
	return item;
}

json_t *json_create_false(void)
{
	json_t *item = json_new_item();
	if (item)
		item->type = JSON_FALSE;
	return item;
}

json_t *json_create_bool(int b)
{
	json_t *item = json_new_item();
	if (item)
		item->type = b ? JSON_TRUE : JSON_FALSE;
	return item;
}

json_t *json_create_number(double num)
{
	json_t *item = json_new_item();
	if (item) {
		item->type = JSON_NUMBER;
		item->valuedouble = num;
		item->valueint = (int)num;
	}
	return item;
}

json_t *json_create_string(const char *string)
{
	json_t *item = json_new_item();
	if (item) {
		item->type = JSON_STRING;
		item->valuestring = strdup(string);
	}
	return item;
}

json_t *json_create_array(void)
{
	json_t *item = json_new_item();
	if (item)
		item->type = JSON_ARRAY;
	return item;
}

json_t *json_create_object(void)
{
	json_t *item = json_new_item();
	if (item)
		item->type = JSON_OBJECT;
	return item;
}

/* create Arrays: */
json_t *json_create_int_array(const int *numbers, int count)
{
	int i;
	json_t *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

json_t *json_create_float_array(const float *numbers, int count)
{
	int i;
	json_t *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

json_t *json_create_double_array(const double *numbers, int count)
{
	int i;
	json_t *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

json_t *json_create_stringArray(const char **strings, int count)
{
	int i;
	json_t *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_string(strings[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

/* duplication */
json_t *json_duplicate(json_t *item, int recurse)
{
	json_t *newitem, *cptr, *nptr = 0, *newchild;
	/* bail on bad ptr */
	if (!item)
		return 0;
	/* create new item */
	newitem = json_new_item();
	if (!newitem)
		return 0;
	/* copy over all vars */
	newitem->type = item->type & (~JSON_IS_REFERENCE), newitem->valueint =
		item->valueint, newitem->valuedouble = item->valuedouble;
	if (item->valuestring) {
		newitem->valuestring = strdup(item->valuestring);
		if (!newitem->valuestring) {
			json_delete(newitem);
			return 0;
		}
	}
	if (item->string) {
		newitem->string = strdup(item->string);
		if (!newitem->string) {
			json_delete(newitem);
			return 0;
		}
	}
	/* if non-recursive, then we're done! */
	if (!recurse)
		return newitem;
	/* walk the ->next chain for the child. */
	cptr = item->child;
	while (cptr) {
		newchild = json_duplicate(cptr, 1);	/* duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {
			json_delete(newitem);
			return 0;
		}
		if (nptr) {
			nptr->next = newchild, newchild->prev = nptr;
			nptr = newchild;
		} /* if newitem->child already set, then crosswire ->prev and ->next and move on */
		else {
			newitem->child = newchild;
			nptr = newchild;
		}						/* set newitem->child and move to it */
		cptr = cptr->next;
	}
	return newitem;
}

void json_minify(char *json)
{
	char *into = json;
	while (*json) {
		if (*json == ' ')
			json++;
		else if (*json == '\t')
			json++;				/* whitespace characters. */
		else if (*json == '\r')
			json++;
		else if (*json == '\n')
			json++;
		else if (*json == '/' && json[1] == '/')
			while (*json && *json != '\n')
				json++;			/* double-slash comments, to end of line. */
		else if (*json == '/' && json[1] == '*') {
			while (*json && !(*json == '*' && json[1] == '/'))
				json++;
			json += 2;
		} /* multiline comments. */
		else if (*json == '\"') {
			*into++ = *json++;
			while (*json && *json != '\"') {
				if (*json == '\\')
					*into++ = *json++;
				*into++ = *json++;
			}
			*into++ = *json++;
		} /* string literals, which are \" sensitive. */
		else
			*into++ = *json++;	/* all other characters. */
	}
	*into = '\0';
}
