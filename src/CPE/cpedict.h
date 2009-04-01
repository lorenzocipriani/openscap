/**
 * @addtogroup CPE
 * @{
 * @addtogroup cpedict CPE Dictionary
 * @{
 *
 *
 * @file cpedict.h
 * \brief Interface to Common Product Enumeration (CPE) Dictionary.
 *
 * See more details at http://nvd.nist.gov/cpe.cfm
 */

/*
 * Copyright 2008 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 */

#ifndef _CPEDICT_H_
#define _CPEDICT_H_

#include <stdlib.h>

#include "cpeuri.h"

/// single check struct
typedef struct cpe_dict_check {
	char *system;		///< system check URI
	char *href;		///< external file reference (NULL if not present)
	char *identifier;	///< test identifier
} cpe_dict_check_t;

/// dictionary item reference
typedef struct cpe_dict_reference {
	char *href;		///< reference URL
	char *content;		///< reference description
} cpe_dict_reference_t;

/// structure representing single CPE dictionary item
typedef struct cpe_dict_item {

	cpe_t *name;		///< CPE name as CPE URI

	char *title;		///< human-readable name of this item

	char **notes;		///< notes as array of strings
	size_t notes_n;		///< number of notes
	size_t notes_alloc_;	///< allocated space for notes (internal)

	cpe_t *depracated;	///< CPE that depracated this one (or NULL)
	char *depracation_date;	///< date of depracation

	cpe_dict_reference_t *references;	///< array of references
	size_t references_n;	///< number of references
	size_t references_alloc_;	///< allocated space for references

	cpe_dict_check_t **check;	///< array of checks to be performed
	size_t check_n;		///< number of checks
	size_t check_alloc_;	///< allocated space for checks

	struct cpe_dict_item *next;	///< pointer to next dictionary item

} cpe_dict_item_t;

/// Structure representing CPE dictionary
typedef struct cpe_dict {

	struct cpe_dict_item *first;	///< pointer to a first dictionary item
	struct cpe_dict_item *last;	///< pointer to a last dictionary item
	size_t item_n;		///< number of items in dictionary
	cpe_t **cpes;		///< pointers to individual CPEs for easy access
	size_t cpes_alloc_;	///< space allocated for cpes, internal

	struct {
		char *product_name;	///< generator software name
		char *product_version;	///< generator software version
		char *schema_version;	///< generator schema version
		char *timestamp;	///< generation date and time
	} generator;		///< generator software info

} cpe_dict_t;

/**
 * Load new CPE dictionary from file
 * @param fname file name of dictionary to load
 * @return new dictionary
 * @retval NULL on failure
 */
cpe_dict_t *cpe_dict_new(const char *fname);

/**
 * Create new empty CPE dictionary
 * @return new dictionary
 * @retval NULL on failure
 */
cpe_dict_t *cpe_dict_new_empty();

/**
 * Add @a item to dictionary @a dict
 *
 * @note item will be deleted as soon as you call cpe_dict_delete on dictionary.
 * @param dict dictionary to add new item to
 * @param item pointer to item to add
 * @return true on success
 */
bool cpe_dict_add_item(cpe_dict_t * dict, cpe_dict_item_t * item);

/**
 * Frees CPE dictionary and its contents
 * @param dict dictionary to be deleted
 */
void cpe_dict_delete(cpe_dict_t * dict);

/**
 * Create new empty dictionary item
 * @return new dictionary item
 * @retval NULL on failure
 */
cpe_dict_item_t *cpe_dictitem_new_empty();

/**
 * Free dictionary item
 * @param item item to be deleted
 */
void cpe_dictitem_delete(cpe_dict_item_t * item);

void cpe_dictcheck_delete(cpe_dict_check_t * check);

#endif				/* _CPEDICT_H_ */

/** @} */
