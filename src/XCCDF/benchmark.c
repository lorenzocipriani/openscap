/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
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

#include <string.h>
#include "item.h"
#include "helpers.h"
#include "xccdf_impl.h"
#include "common/_error.h"
#include <text.h>

#define XCCDF_SUPPORTED "1.1.4"

struct xccdf_backref {
	struct xccdf_item **ptr;	// pointer to a pointer that is supposed to be pointing to an item with id 'id'
	xccdf_type_t type;	// expected item type
	char *id;		// id
};

struct xccdf_benchmark *xccdf_benchmark_import(struct oscap_import_source *source)
{
	xmlTextReaderPtr reader = xmlReaderForFile(oscap_import_source_get_name(source), NULL, 0);
	if (!reader)
		return NULL;
	while (xmlTextReaderRead(reader) == 1 && xmlTextReaderNodeType(reader) != 1) ;
	struct xccdf_item *benchmark = XITEM(xccdf_benchmark_new());
	xccdf_benchmark_parse(benchmark, reader);
	xmlFreeTextReader(reader);
	return XBENCHMARK(benchmark);
}

struct xccdf_benchmark *xccdf_benchmark_new(void)
{
	struct xccdf_item *bench = xccdf_item_new(XCCDF_BENCHMARK, NULL);
    // lists
	bench->sub.benchmark.rear_matter  = oscap_list_new();
	bench->sub.benchmark.front_matter = oscap_list_new();
	bench->sub.benchmark.notices = oscap_list_new();
	bench->sub.benchmark.models = oscap_list_new();
	bench->sub.benchmark.content = oscap_list_new();
	bench->sub.benchmark.values = oscap_list_new();
	bench->sub.benchmark.plain_texts = oscap_list_new();
	bench->sub.benchmark.profiles = oscap_list_new();
	bench->sub.benchmark.results = oscap_list_new();
    // hash tables
	bench->sub.benchmark.dict = oscap_htable_new();
	return XBENCHMARK(bench);
}

bool xccdf_benchmark_parse(struct xccdf_item * benchmark, xmlTextReaderPtr reader)
{
	XCCDF_ASSERT_ELEMENT(reader, XCCDFE_BENCHMARK);
	assert(benchmark != NULL);
	if (benchmark->type != XCCDF_BENCHMARK)
		return false;

	if (!xccdf_item_process_attributes(benchmark, reader)) {
		xccdf_benchmark_free(XBENCHMARK(benchmark));
		return false;
	}
	benchmark->sub.benchmark.style = xccdf_attribute_copy(reader, XCCDFA_STYLE);
	benchmark->sub.benchmark.style_href = xccdf_attribute_copy(reader, XCCDFA_STYLE_HREF);
	if (xccdf_attribute_has(reader, XCCDFA_RESOLVED))
		benchmark->item.flags.resolved = xccdf_attribute_get_bool(reader, XCCDFA_RESOLVED);

	int depth = oscap_element_depth(reader) + 1;

	while (oscap_to_start_element(reader, depth)) {
		switch (xccdf_element_get(reader)) {
		case XCCDFE_NOTICE:
				oscap_list_add(benchmark->sub.benchmark.notices, xccdf_notice_new_parse(reader));
				break;
		case XCCDFE_FRONT_MATTER:
				oscap_list_add(benchmark->sub.benchmark.front_matter, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
			break;
		case XCCDFE_REAR_MATTER:
				oscap_list_add(benchmark->sub.benchmark.rear_matter, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
			break;
		case XCCDFE_METADATA:
			if (!benchmark->sub.benchmark.metadata)
				benchmark->sub.benchmark.metadata = oscap_get_xml(reader);
			break;
		case XCCDFE_PLATFORM:
			oscap_list_add(benchmark->item.platforms, xccdf_attribute_copy(reader, XCCDFA_IDREF));
			break;
		case XCCDFE_MODEL:
			oscap_list_add(benchmark->sub.benchmark.models, xccdf_model_new_xml(reader));
			break;
		case XCCDFE_PLAIN_TEXT:{
				const char *id = xccdf_attribute_get(reader, XCCDFA_ID);
				const char *data = oscap_element_string_get(reader);
				if (id && data)
                    oscap_list_add(benchmark->sub.benchmark.plain_texts,
                                xccdf_plain_text_new_fill(id, data));
				break;
			}
		case XCCDFE_PROFILE:
			oscap_list_add(benchmark->sub.benchmark.profiles, xccdf_profile_parse(reader, benchmark));
			break;
		case XCCDFE_GROUP:
		case XCCDFE_RULE:
			xccdf_content_parse(reader, benchmark);
			break;
		case XCCDFE_VALUE:
			oscap_list_add(benchmark->sub.benchmark.values, xccdf_value_parse(reader, benchmark));
			break;
		case XCCDFE_TESTRESULT:
			xccdf_benchmark_add_result(XBENCHMARK(benchmark), xccdf_result_new_parse(reader));
			break;
		default:
			xccdf_item_process_element(benchmark, reader);
		}
		xmlTextReaderRead(reader);
	}

	return true;
}

int xccdf_benchmark_export(struct xccdf_benchmark *benchmark, struct oscap_export_target *target)
{
	__attribute__nonnull__(target);

	int retcode = 0;

	LIBXML_TEST_VERSION;

	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	if (doc == NULL) {
		oscap_setxmlerr(xmlGetLastError());
		return -1;
	}

	xccdf_benchmark_to_dom(benchmark, doc, NULL, NULL);

	retcode = xmlSaveFormatFileEnc(oscap_export_target_get_name(target), doc, oscap_export_target_get_encoding(target), 1);
	if (retcode < 1)
		oscap_setxmlerr(xmlGetLastError());

	xmlFreeDoc(doc);

	return retcode;
}

xmlNode *xccdf_benchmark_to_dom(struct xccdf_benchmark *benchmark, xmlDocPtr doc,
				xmlNode *parent, void *user_args)
{
	xmlNodePtr root_node = NULL;

	if (parent) {
		root_node = xccdf_item_to_dom(XITEM(benchmark), doc, parent);
	} else {
		root_node = xccdf_item_to_dom(XITEM(benchmark), doc, parent);
		xmlDocSetRootElement(doc, root_node);
	}
	xmlNs *ns_xccdf = xmlNewNs(root_node, XCCDF_BASE_NAMESPACE, NULL);

	xmlSetNs(root_node, ns_xccdf);

	/* Handle attributes */
	if (xccdf_benchmark_get_resolved(benchmark))
		xmlNewProp(root_node, BAD_CAST "resolved", BAD_CAST "1");
	else
		xmlNewProp(root_node, BAD_CAST "resolved", BAD_CAST "0");

	const char *style = xccdf_benchmark_get_style(benchmark);
	if (style)
		xmlNewProp(root_node, BAD_CAST "style", BAD_CAST style);

	const char *style_href = xccdf_benchmark_get_style_href(benchmark);
	if (style_href)
		xmlNewProp(root_node, BAD_CAST "style-href", BAD_CAST style_href);

	/* In spec but not in OpenSCAP
	const char *lang = xccdf_benchmark_get_lang(benchmark);
	if (lang)
		xmlNewProp(root_node, BAD_CAST "xml:lang", BAD_CAST lang);*/

	/* Handle children */
	struct xccdf_notice_iterator *notices = xccdf_benchmark_get_notices(benchmark);
	while (xccdf_notice_iterator_has_more(notices)) {
		struct xccdf_notice *notice = xccdf_notice_iterator_next(notices);
                struct oscap_text *text = xccdf_notice_get_text(notice);
		xmlNode *notice_node = xmlNewChild(root_node, ns_xccdf, BAD_CAST "notice", BAD_CAST oscap_text_get_text(text));

		const char *lang = oscap_text_get_lang(text);
		if (lang)
			xmlNewProp(notice_node, BAD_CAST "xml:lang", BAD_CAST lang);

		const char *id = xccdf_notice_get_id(notice);
		if (id)
			xmlNewProp(notice_node, BAD_CAST "id", BAD_CAST id);
	}
	xccdf_notice_iterator_free(notices);

	struct oscap_text_iterator *fronts = xccdf_benchmark_get_front_matter(benchmark);
	while (oscap_text_iterator_has_more(fronts)) {
		struct oscap_text *front = oscap_text_iterator_next(fronts);
		xmlNode *front_node = xmlNewChild(root_node, ns_xccdf, BAD_CAST "front-matter", BAD_CAST oscap_text_get_text(front));

		const char *lang = oscap_text_get_lang(front);
		if (lang)
			xmlNewProp(front_node, BAD_CAST "xml:lang", BAD_CAST lang);

		/* TODO: Dublin Core */
	}
	oscap_text_iterator_free(fronts);

	struct oscap_text_iterator *rears = xccdf_benchmark_get_rear_matter(benchmark);
	while (oscap_text_iterator_has_more(rears)) {
		struct oscap_text *rear = oscap_text_iterator_next(rears);
		xmlNode *rear_node = xmlNewChild(root_node, ns_xccdf, BAD_CAST "rear-matter", BAD_CAST oscap_text_get_text(rear));

		const char *lang = oscap_text_get_lang(rear);
		if (lang)
			xmlNewProp(rear_node, BAD_CAST "xml:lang", BAD_CAST lang);

		/* TODO: Dublin Core */
	}
	oscap_text_iterator_free(rears);

	struct oscap_string_iterator *platforms = xccdf_benchmark_get_platforms(benchmark);
	while (oscap_string_iterator_has_more(platforms)) {
		xmlNode *platform_node = xmlNewChild(root_node, ns_xccdf, BAD_CAST "platform", NULL);

		const char *idref = oscap_string_iterator_next(platforms);
		if (idref)
			xmlNewProp(platform_node, BAD_CAST "idref", BAD_CAST idref);
	}
	oscap_string_iterator_free(platforms);

	const char *metadata = xccdf_benchmark_get_metadata(benchmark);
	if (metadata) {
		xmlNode *metadata_node = xmlNewChild(root_node, ns_xccdf, BAD_CAST "metadata", NULL);
		xmlNewChild(metadata_node, ns_xccdf, NULL, BAD_CAST metadata);
	}

	struct xccdf_profile_iterator *profiles = xccdf_benchmark_get_profiles(benchmark);
	while (xccdf_profile_iterator_has_more(profiles)) {
		struct xccdf_profile *profile = xccdf_profile_iterator_next(profiles);
		xccdf_item_to_dom(XITEM(profile), doc, root_node);
	}
	xccdf_profile_iterator_free(profiles);

	struct xccdf_value_iterator *values = xccdf_benchmark_get_values(benchmark);
	while (xccdf_value_iterator_has_more(values)) {
		struct xccdf_value *value = xccdf_value_iterator_next(values);
		xccdf_item_to_dom(XITEM(value), doc, root_node);
	}
	xccdf_value_iterator_free(values);

	struct xccdf_item_iterator *items = xccdf_benchmark_get_content(benchmark);
	while (xccdf_item_iterator_has_more(items)) {
		struct xccdf_item *item = xccdf_item_iterator_next(items);
		if (XBENCHMARK(xccdf_item_get_parent(item)) == benchmark)
			xccdf_item_to_dom(item, doc, root_node);
	}
	xccdf_item_iterator_free(items);

	struct xccdf_result_iterator *results = xccdf_benchmark_get_results(benchmark);
	while (xccdf_result_iterator_has_more(results)) {
		struct xccdf_result *result = xccdf_result_iterator_next(results);
		xccdf_item_to_dom(XITEM(result), doc, root_node);
	}
	xccdf_result_iterator_free(results);

	return root_node;
}

void xccdf_benchmark_dump(struct xccdf_benchmark *benchmark)
{
	struct xccdf_item *bench = XITEM(benchmark);
	printf("Benchmark : %s\n", (bench ? bench->item.id : "(NULL)"));
	if (bench) {
		xccdf_item_print(bench, 1);
		printf("  front m.");
		xccdf_print_textlist(xccdf_benchmark_get_front_matter(benchmark), 2, 80, "...");
		printf("  rear m.");
		xccdf_print_textlist(xccdf_benchmark_get_rear_matter(benchmark), 2, 80, "...");
		printf("  profiles");
		oscap_list_dump(bench->sub.benchmark.profiles, xccdf_profile_dump, 2);
		printf("  values");
		oscap_list_dump(bench->sub.benchmark.values, xccdf_value_dump, 2);
		printf("  content");
		oscap_list_dump(bench->sub.benchmark.content, xccdf_item_dump, 2);
		printf("  results");
		oscap_list_dump(bench->sub.benchmark.results, (oscap_dump_func) xccdf_result_dump, 2);
	}
}

void xccdf_benchmark_free(struct xccdf_benchmark *benchmark)
{
	if (benchmark) {
		struct xccdf_item *bench = XITEM(benchmark);
		oscap_free(bench->sub.benchmark.style);
		oscap_free(bench->sub.benchmark.style_href);
		oscap_free(bench->sub.benchmark.metadata);
		oscap_list_free(bench->sub.benchmark.front_matter, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(bench->sub.benchmark.rear_matter, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(bench->sub.benchmark.notices, (oscap_destruct_func) xccdf_notice_free);
		oscap_list_free(bench->sub.benchmark.models, (oscap_destruct_func) xccdf_model_free);
		oscap_list_free(bench->sub.benchmark.content, (oscap_destruct_func) xccdf_item_free);
		oscap_list_free(bench->sub.benchmark.values, (oscap_destruct_func) xccdf_value_free);
		oscap_list_free(bench->sub.benchmark.results, (oscap_destruct_func) xccdf_result_free);
		oscap_list_free(bench->sub.benchmark.plain_texts, (oscap_destruct_func) xccdf_plain_text_free);
		oscap_htable_free(bench->sub.benchmark.dict, NULL);
		oscap_list_free(bench->sub.benchmark.profiles, (oscap_destruct_func) xccdf_profile_free);
		xccdf_item_release(bench);
	}
}

XCCDF_ACCESSOR_STRING(benchmark, metadata)
XCCDF_ACCESSOR_STRING(benchmark, style)
XCCDF_ACCESSOR_STRING(benchmark, style_href)
XCCDF_LISTMANIP_TEXT(benchmark, front_matter, front_matter)
XCCDF_LISTMANIP_TEXT(benchmark, rear_matter, rear_matter)
XCCDF_LISTMANIP(benchmark, notice, notices)
XCCDF_LISTMANIP(benchmark, model, models)
XCCDF_BENCHMARK_IGETTER(item, content)
XCCDF_BENCHMARK_IGETTER(result, results)
XCCDF_BENCHMARK_IGETTER(value, values)
XCCDF_BENCHMARK_IGETTER(profile, profiles)
XCCDF_ITERATOR_GEN_S(notice)
XCCDF_ITERATOR_GEN_S(model)
XCCDF_ITERATOR_GEN_S(profile)
XCCDF_LISTMANIP(benchmark, plain_text, plain_texts)
XCCDF_HTABLE_GETTER(struct xccdf_item *, benchmark, item, sub.benchmark.dict)
XCCDF_STATUS_CURRENT(benchmark)
OSCAP_ITERATOR_GEN(xccdf_plain_text)


XCCDF_ITEM_ADDER_REG(benchmark, result, results)
XCCDF_ITEM_ADDER_REG(benchmark, rule, content)
XCCDF_ITEM_ADDER_REG(benchmark, group, content)
XCCDF_ITEM_ADDER_REG(benchmark, value, values)
XCCDF_ITEM_ADDER_REG(benchmark, profile, profiles)

bool xccdf_benchmark_add_content(struct xccdf_benchmark *bench, struct xccdf_item *item)
{
	if (item == NULL) return false;
	switch (xccdf_item_get_type(item)) {
		case XCCDF_RULE:  return xccdf_benchmark_add_rule(bench, XRULE(item));
		case XCCDF_GROUP: return xccdf_benchmark_add_group(bench, XGROUP(item));
		case XCCDF_VALUE: return xccdf_benchmark_add_value(bench, XVALUE(item));
		default: return false;
	}
}


const char *xccdf_benchmark_get_plain_text(const struct xccdf_benchmark *bench, const char *id)
{
    assert(bench != NULL);

    OSCAP_FOR(xccdf_plain_text, cur, xccdf_benchmark_get_plain_texts(bench)) {
        if (oscap_streq(cur->id, id)) {
            xccdf_plain_text_iterator_free(cur_iter);
            return cur->text;
        }
    }
    return NULL;
}

struct xccdf_notice *xccdf_notice_new(void)
{
    struct xccdf_notice *notice = oscap_calloc(1, sizeof(struct xccdf_notice));
    notice->text = oscap_text_new_full(XCCDF_TEXT_NOTICE, NULL, NULL);
    return NULL;
}

struct xccdf_notice *xccdf_notice_new_parse(xmlTextReaderPtr reader)
{
    struct xccdf_notice *notice = oscap_calloc(1, sizeof(struct xccdf_notice));
    notice->id = xccdf_attribute_copy(reader, XCCDFA_ID);
    notice->text = oscap_text_new_parse(XCCDF_TEXT_NOTICE, reader);
    return notice;
}

void xccdf_notice_dump(struct xccdf_notice *notice, int depth)
{
	xccdf_print_depth(depth);
	printf("%.20s: ", xccdf_notice_get_id(notice));
	xccdf_print_max_text(xccdf_notice_get_text(notice), 50, "...");
	printf("\n");
}

void xccdf_notice_free(struct xccdf_notice *notice)
{
	if (notice) {
		oscap_free(notice->id);
		oscap_text_free(notice->text);
		oscap_free(notice);
	}
}

OSCAP_ACCESSOR_STRING(xccdf_notice, id)
OSCAP_ACCESSOR_TEXT(xccdf_notice, text)

void xccdf_cleanup(void)
{
	xmlCleanupParser();
}

const char * xccdf_benchmark_supported(void)
{
    return XCCDF_SUPPORTED;
}

struct xccdf_group *xccdf_benchmark_append_new_group(const struct xccdf_benchmark *benchmark, const char *id)
{
    //TODO: not implemented
    oscap_seterr(OSCAP_EFAMILY_OSCAP, OSCAP_ENOTIMPL, "This feature is not implemented");
    return NULL;
}
struct xccdf_value *xccdf_benchmark_append_new_value(const struct xccdf_benchmark *benchmark, const char *id, xccdf_value_type_t type)
{
    //TODO: not implemented
    oscap_seterr(OSCAP_EFAMILY_OSCAP, OSCAP_ENOTIMPL, "This feature is not implemented");
    return NULL;
}
struct xccdf_rule *xccdf_benchmark_append_new_rule(const struct xccdf_benchmark *benchmark, const char *id)
{
    //TODO: not implemented
    oscap_seterr(OSCAP_EFAMILY_OSCAP, OSCAP_ENOTIMPL, "This feature is not implemented");
    return NULL;
}

static const size_t XCCDF_ID_SIZE = 32;

char *xccdf_benchmark_gen_id(struct xccdf_benchmark *benchmark, const char *prefix)
{
	assert(prefix != NULL);

	char buff[XCCDF_ID_SIZE];
	memset(buff, 0, XCCDF_ID_SIZE);
	int i = 0;

	do {
		snprintf(buff, XCCDF_ID_SIZE - 1, "%s%03d", prefix, ++i);
	} while (xccdf_benchmark_get_item(benchmark, buff) != NULL);

	return oscap_strdup(buff);
}

bool xccdf_add_item(struct oscap_list *list, struct xccdf_item *parent, struct xccdf_item *item, const char *prefix)
{
	assert(list != NULL);
	assert(item != NULL);

    if (parent == NULL)
        return false;

	struct xccdf_benchmark *bench = xccdf_item_get_benchmark(parent);

	if (bench != NULL) {
		if (xccdf_item_get_id(item) == NULL)
			item->item.id = xccdf_benchmark_gen_id(bench, prefix);

		if (xccdf_benchmark_register_item(bench, item)) {
			item->item.parent = parent;
			return oscap_list_add(list, item);
		}
	}
	else return true;

    return false;
}

bool xccdf_benchmark_register_item(struct xccdf_benchmark *benchmark, struct xccdf_item *item)
{
	assert(benchmark != NULL);

	if (item == NULL || xccdf_item_get_id(item) == NULL)
		return false;

	if (xccdf_item_get_benchmark(item) != NULL)
		return xccdf_item_get_benchmark(item) == benchmark; // already registered

	return oscap_htable_add(XITEM(benchmark)->sub.benchmark.dict, xccdf_item_get_id(item), item);
}

bool xccdf_benchmark_unregister_item(struct xccdf_item *item)
{
	if (item == NULL)
		return false;

	if (xccdf_item_get_benchmark(item) == NULL)
		return false;

	assert(xccdf_benchmark_get_item(xccdf_item_get_benchmark(item), xccdf_item_get_id(item)) == item);

	return oscap_htable_detach(XITEM(xccdf_item_get_benchmark(item))->sub.benchmark.dict, xccdf_item_get_id(item)) != NULL;
}

bool xccdf_benchmark_rename_item(struct xccdf_item *item, const char *newid)
{
	if (item == NULL)
		return false;

	struct xccdf_item *bench = XITEM(xccdf_item_get_benchmark(item));

	if (bench != NULL) {
		if (newid != NULL && xccdf_benchmark_get_item(XBENCHMARK(bench), newid) != NULL)
			return false; // ID already assigned
		xccdf_benchmark_unregister_item(item);

		if (newid != NULL)
			oscap_htable_add(bench->sub.benchmark.dict, newid, item);
	}

	oscap_free(item->item.id);
	item->item.id = oscap_strdup(newid);

	return true;
}


struct xccdf_plain_text *xccdf_plain_text_new(void)
{
    return oscap_calloc(1, sizeof(struct xccdf_plain_text));
}

struct xccdf_plain_text *xccdf_plain_text_new_fill(const char *id, const char *text)
{
    struct xccdf_plain_text *plain = xccdf_plain_text_new();
    plain->id = oscap_strdup(id);
    plain->text = oscap_strdup(text);
    return plain;
}

void xccdf_plain_text_free(struct xccdf_plain_text *plain)
{
    if (plain != NULL) {
        oscap_free(plain->id);
        oscap_free(plain->text);
        oscap_free(plain);
    }
}

OSCAP_ACCESSOR_STRING(xccdf_plain_text, id)
OSCAP_ACCESSOR_STRING(xccdf_plain_text, text)

