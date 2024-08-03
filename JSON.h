#ifndef _JSON_H
#define _JSON_H
/**
MIT License

Copyright (c) 2024 aymanpopje

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#if __STDC_NO_VLA__ != 1
#   define JSON_ARRAY_BOUND(...) __VA_ARGS__
#else
#   define JSON_ARRAY_BOUND(...) /**/
#endif

#if defined(JSON_SOURCE) && defined(JSON_DECLS_ONLY)
#   error "`JSON_SOURCE` and `JSON_DECLS_ONLY` may not be used together"
#endif

#if defined(JSON_SOURCE)
#   define JSON_API extern
#else
#   define JSON_API static
#endif

typedef enum /*: unsigned char */{
    JSON_NULL_TYPE = 0U,
    JSON_BOOLEAN_TYPE,
    JSON_NUMBER_TYPE,
    JSON_STRING_TYPE,
    JSON_ARRAY_TYPE,
    JSON_OBJECT_TYPE,
} JSON_type_t;

struct JSON {
    unsigned offset; // the offset from the start of the string
    unsigned size; // the number of characters large this node is within the string
    unsigned treesize; // total number of child nodes
    JSON_type_t type; // the datatype
};

typedef enum {
    JSON_EOK = 0,
    
    // Parsing errors
    JSON_EEOT, // expected token, found end of text
    JSON_ENOMEM, // token buffer is full
    JSON_EILL, // unrecognized token
    JSON_EINVAL, // unexpected token
    
    // Number parsing errors
    JSON_ENDIG, // expected digits

    // String parsing errors
    JSON_ESTERM, // string was not terminated by a `"`
    JSON_ESILL, // illegal character in string (control character)
    JSON_ESESC, // illegal escape sequence
} JSON_errno_t;

JSON_API JSON_errno_t JSON(unsigned baseoffset, unsigned jsonsize, const char json[JSON_ARRAY_BOUND(static jsonsize)], unsigned destsize, struct JSON dest[JSON_ARRAY_BOUND(destsize)]);

JSON_API const char *JSON_strerror(JSON_errno_t e);

#ifndef JSON_DECLS_ONLY
static const char *whitespace(const char *begin, const char *end) {
    while(begin != end && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')) { ++begin; }
    return begin;
}

JSON_API JSON_errno_t JSON(unsigned baseoffset, unsigned jsonsize, const char json[JSON_ARRAY_BOUND(static jsonsize)], unsigned destsize, struct JSON dest[JSON_ARRAY_BOUND(destsize)]) {
    const char *pos = json, *end = &json[jsonsize];
    unsigned destpos = 0;

    pos = whitespace(pos, end);

    if(pos == end) { return JSON_EEOT; }
    if(destpos == destsize) { return JSON_ENOMEM; }

    switch(*pos) {
    case '{': // object
        {
            const char *start = pos;
            ++pos;
            pos = whitespace(pos, end);

            struct JSON *object = &dest[destpos];
            
            ++destpos;
            if(pos == end) { return JSON_EEOT; }
            if(*pos == '}') {
                ++pos;
                *object = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .treesize = 0, .type = JSON_OBJECT_TYPE};
                return JSON_EOK;
            }

            unsigned treesize = 0;
            while(1) {
                pos = whitespace(pos, end);

                // key
                {
                    JSON_errno_t e = JSON(baseoffset + pos - json, end - pos, pos, destsize - destpos, &dest[destpos]);
                    if(e != JSON_EOK) { return e; }
                    if(dest[destpos].type != JSON_STRING_TYPE) { return JSON_EINVAL; }

                    pos += dest[destpos].size;
                    ++destpos;
                    ++treesize;
                }

                pos = whitespace(pos, end);

                // key-value seperator
                if(pos == end) { return JSON_EEOT; }
                if(*pos != ':') { return JSON_EINVAL; }
                ++pos;

                pos = whitespace(pos, end);
                
                // value
                {
                    JSON_errno_t e = JSON(baseoffset + pos - json, end - pos, pos, destsize - destpos, &dest[destpos]);
                    if(e != JSON_EOK) { return e; }

                    pos += dest[destpos].size;
                    treesize += dest[destpos].treesize + 1;
                    destpos += dest[destpos].treesize + 1;
                }

                pos = whitespace(pos, end);

                if(pos == end) { return JSON_EEOT; }
                if(*pos == ',') { ++pos; continue; }

                break;
            };

            if(*pos != '}') { return JSON_EINVAL; };
            ++pos;

            *object = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .treesize = treesize, .type = JSON_OBJECT_TYPE};
            return JSON_EOK;
        }

    case '[': // array
        {
            const char *start = pos;
            ++pos;
            pos = whitespace(pos, end);

            struct JSON *array = &dest[destpos];
            
            ++destpos;
            if(pos == end) { return JSON_EEOT; }
            if(*pos == ']') {
                ++pos;
                *array = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .treesize = 0, .type = JSON_ARRAY_TYPE};
                return JSON_EOK;
            }

            unsigned treesize = 0;
            while(1) {
                pos = whitespace(pos, end);
                
                // element
                {
                    JSON_errno_t e = JSON(baseoffset + pos - json, end - pos, pos, destsize - destpos, &dest[destpos]);
                    if(e != JSON_EOK) { return e; }

                    pos += dest[destpos].size;
                    treesize += dest[destpos].treesize + 1;
                    destpos += dest[destpos].treesize + 1;
                }

                pos = whitespace(pos, end);

                if(pos == end) { return JSON_EEOT; }
                if(*pos == ',') { ++pos; continue; }

                break;
            };

            if(*pos != ']') { return JSON_EINVAL; };
            ++pos;

            *array = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .treesize = treesize, .type = JSON_ARRAY_TYPE};
            return JSON_EOK;
        }

    case '"': // string
        {
            const char *start = pos;

            pos++;
            while(1) {
                // missing, terminating double-quote
                if(pos == end) {
                    return JSON_ESTERM;
                }

                // terminating double-quote
                if(*pos == '"') {
                    ++pos;
                    break;
                }

                // escape sequences
                if(*pos == '\\' && pos < end) {
                    ++pos;
                    switch(*pos) {
                    case '\"':
                    case '\\':
                    case '/':
                    case 'b':
                    case 'f':
                    case 'n':
                    case 'r':
                    case 't':
                        ++pos;
                        break;

                    case 'u':
                        ++pos;
                        if(&pos[sizeof("HHHH") - 1] > end) { return JSON_ESESC; }
                        
                        for(int i = 0; i != 4; ++i, ++pos) {
                            if(!((*pos >= '0' && *pos <= '9') || (*pos >= 'a' && *pos <= 'z') || (*pos >= 'A' && *pos <= 'Z'))) {
                                return JSON_ESESC;
                            }
                        }
                        break;

                    default:
                        return JSON_ESESC;
                    }

                    continue;
                }

                // control character are disallowed
                if((*pos >= 0x00 && *pos <= 0x1F) || *pos == 0x7F) { return JSON_ESILL; }
                ++pos;
            }

            dest[destpos] = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .type = JSON_STRING_TYPE};
            destpos++;
            return JSON_EOK;
        }

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': // number
        {
            const char *start = pos;

            if(*pos == '-') { // sign
                ++pos;
                if(pos == end) {
                    return JSON_ENDIG;
                }
            }

            if(pos == end) {
                return JSON_ENDIG;
            }

            if(*pos == '0') { // integer
                ++pos;
            } else if(*pos >= '1' && *pos <= '9') {
                while(pos != end && *pos >= '0' && *pos <= '9') {
                    ++pos;
                }
            } else {
                return JSON_ENDIG;
            }

            if(pos != end && *pos == '.') { // fraction
                ++pos;
                if(pos == end || *pos < '0' || *pos > '9') {
                    ++pos;
                    return JSON_ENDIG;
                }

                while(pos != end && *pos >= '0' && *pos <= '9') {
                    ++pos;
                }
            }

            if(pos != end && (*pos == 'e' || *pos == 'E')) { // exponent
                ++pos;
                if(pos == end) { return JSON_ENDIG; }

                if(*pos == '-') { ++pos; }
                if(pos == end) { return JSON_ENDIG; }

                if(pos == end || *pos < '0' || *pos > '9') {
                    return JSON_ENDIG;
                }

                while(pos != end && *pos >= '0' && *pos <= '9') {
                    ++pos;
                }
            }

            dest[destpos] = (struct JSON){.offset = start - json + baseoffset, .size = pos - start, .type = JSON_NUMBER_TYPE};
            destpos++;
            return JSON_EOK;
        }

    case 't': // true
        if(pos + (sizeof("true") - 1) > end) { return JSON_EILL; }
        if(pos[1] == 'r' && pos[2] == 'u' && pos[3] == 'e') {
            pos += (sizeof("true") - 1);

            dest[destpos] = (struct JSON){.offset = (pos - json) - (sizeof("true") - 1) + baseoffset, .size = (sizeof("true") - 1), .type = JSON_BOOLEAN_TYPE};
            destpos++;
            return JSON_EOK;
        }
        return JSON_EILL;

    case 'f': // false
        if(pos + (sizeof("false") - 1) > end) { return JSON_EILL; }
        if(pos[1] == 'a' && pos[2] == 'l' && pos[3] == 's' && pos[4] == 'e') {
            pos += (sizeof("false") - 1);

            dest[destpos] = (struct JSON){.offset = (pos - json) - (sizeof("false") - 1) + baseoffset, .size = (sizeof("false") - 1), .type = JSON_BOOLEAN_TYPE};
            destpos++;
            return JSON_EOK;
        }
        return JSON_EILL;

    case 'n': // null
        if(pos + (sizeof("null") - 1) > end) { return JSON_EILL; }
        if(pos[1] == 'u' && pos[2] == 'l' && pos[3] == 'l') {
            pos += (sizeof("null") - 1);

            dest[destpos] = (struct JSON){.offset = (pos - json) - (sizeof("null") - 1) + baseoffset, .size = (sizeof("null") - 1), .type = JSON_NULL_TYPE};
            destpos++;
            return JSON_EOK;
        }
        return JSON_EILL;
    

    default:
        return JSON_EILL;
    }

    return JSON_EEOT;
}

JSON_API const char *JSON_strerror(JSON_errno_t e) {
    static const char *const errorlist[] = {
        [JSON_EOK] = "Ok",
        [JSON_EEOT] = "expected token, found end of text",
        [JSON_ENOMEM] = "token buffer is full",
        [JSON_EILL] = "unrecognized token",
        [JSON_EINVAL] = "unexpected token",
        [JSON_ENDIG] = "expected digits",
        [JSON_ESTERM] = "string was not terminated by a `\"`",
        [JSON_ESILL] = "illegal character in string (control character)",
        [JSON_ESESC] = "illegal escape sequence",
    };

    return ((unsigned)e >= sizeof(errorlist) / sizeof(errorlist[0])) ? 0 : errorlist[(unsigned) e];
}
#endif

#endif /* _JSON_H */
