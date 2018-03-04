#ifndef OUI_H
#define OUI_H

#include <sys/types.h>

/*
 * An Organizationally Unique Identifier, in other words the first 24-bits
 * of a 48-bit MAC address that are assigned to a particular organization.
 *
 * 8 bits are wasted by using uint32_t but it's more convenient than
 * using an array.
 *
 * https://en.wikipedia.org/wiki/Organizationally_unique_identifier
 */
typedef uint32_t Oui;

#endif
