
#ifndef COMMON_H_
#define COMMON_H_

// Function over char
// #define TO_LOWER(c)       (unsigned char)(c | 0x20)
#define TO_LOWER(c)    (((c) >= 'A' && (c) <= 'Z') ? ((c) | 0x20) : (c))
#define TO_NUM(c)      ((c) - '0')
// #define IS_ALPHA(c)    (TO_LOWER(c) >= 'a' && TO_LOWER(c) <= 'z')
#define IS_ALPHA(c)    (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define IS_NUM(c)      ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c) (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)      (IS_NUM(c) || (TO_LOWER(c) >= 'a' && TO_LOWER(c) <= 'f'))

// Function over null-terminated string
// In-place string lowercase converter (skips non-alphas like numbers)
#define STR_TO_LOWER(s) do { \
    for (char *p = (s); *p; p++) { \
        if (IS_ALPHA(*p)) { \
            *p = TO_LOWER(*p); \
        } \
    } \
} while(0)
// Convert null-terminated string to size_t
#define STR_TO_NUM(s, result) do { \
    for (; *s; s++) { \
        if (IS_NUM(*s)) { \
            result = (result * 10) + TO_NUM(*s); \
        } \
    } \
} while(0)

#define HOST "127.0.0.1"
#define PORT 8000

#endif // COMMON_H_
