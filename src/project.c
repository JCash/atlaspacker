// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#include <atlaspacker/project.h>
#include <atlaspacker/file.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ParseSources(apProject* p, cJSON* sources)
{
	p->sources = 0;
	p->num_sources = 0;

	if (!sources || !cJSON_IsArray(sources))
		return;

	int array_size = cJSON_GetArraySize(sources);
	p->sources = (const char**)malloc(array_size * sizeof(const char*));

	cJSON* source;
	p->num_sources = 0;
    cJSON_ArrayForEach(source, sources)
    {
    	if (cJSON_IsString(source))
    	{
    		p->sources[p->num_sources++] = strdup(cJSON_GetStringValue(source));
    	}
    }
}

static int GetInt(cJSON* object, const char* name, int default_value)
{
	cJSON* var = cJSON_GetObjectItemCaseSensitive(object, name);
	if (!var || !cJSON_IsNumber(var))
		return default_value;
	return (int)cJSON_GetNumberValue(var);
}

static const char* GetString(cJSON* object, const char* name, const char* default_value)
{
    cJSON* var = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!var || !cJSON_IsString(var))
        return default_value;
    return cJSON_GetStringValue(var);
}

// static bool GetBool(cJSON* object, const char* name, bool default_value)
// {
// 	cJSON* var = cJSON_GetObjectItemCaseSensitive(object, name);
// 	if (!var || !cJSON_IsBool(var))
// 		return default_value;
// 	return cJSON_IsTrue(var);
// }

static void ParseGeneralPackerOptions(apProject* p, cJSON* packer)
{
    apOptions* options = &p->options;

    options->page_size = GetInt(packer, "page_size", options->page_size);

    // Should the validation be done by each packer?
    if (options->page_size < 1)
        options->page_size = 1;
}

static void ParseTilePackerOptions(apProject* p, cJSON* packer)
{
	p->packer_type = PT_TILEPACKER;

	p->options_tp.no_rotate       = GetInt(packer, "no_rotate", p->options_tp.no_rotate);
	p->options_tp.tile_size 	  = GetInt(packer, "tile_size", p->options_tp.tile_size);
	p->options_tp.padding 		  = GetInt(packer, "padding", p->options_tp.padding);
	p->options_tp.alpha_threshold = GetInt(packer, "alpha_threshold", p->options_tp.alpha_threshold);

	// Should the validation be done by each packer?
	p->options_tp.no_rotate = p->options_tp.no_rotate ? 1 : 0;
	if (p->options_tp.tile_size < 1)
		p->options_tp.tile_size = 1;
	if (p->options_tp.padding < 0)
		p->options_tp.padding = 0;
	if (p->options_tp.alpha_threshold < 0)
		p->options_tp.alpha_threshold = 0;
	if (p->options_tp.alpha_threshold > 255)
		p->options_tp.alpha_threshold = 255;
}

static void ParseBinPackerOptions(apProject* p, cJSON* packer)
{
    apBinPackerOptions* options = &p->options_bp;

    p->packer_type = PT_BINPACKER;

    // Should the validation be done by each packer?
    options->mode       = GetInt(packer, "mode", options->mode);
    options->no_rotate  = GetInt(packer, "no_rotate", options->no_rotate) ? 1 : 0;

    if (options->mode != 0)
        options->mode = 0; // Until we support more modes
}

static void ParsePacker(apProject* p, cJSON* packer)
{
	if (!packer || !cJSON_IsObject(packer))
		return;

    cJSON* options = cJSON_GetObjectItemCaseSensitive(packer, "options");
    if (options)
        ParseGeneralPackerOptions(p, options);

	cJSON* type = cJSON_GetObjectItemCaseSensitive(packer, "type");
	if (!type || !cJSON_IsString(type))
		return;

	const char* type_str = cJSON_GetStringValue(type);

    cJSON* packer_type_object = cJSON_GetObjectItemCaseSensitive(packer, type_str);
	if (strcmp("tilepacker", type_str) == 0)
		ParseTilePackerOptions(p, packer_type_object);
    else if (strcmp("binpacker", type_str) == 0)
        ParseBinPackerOptions(p, packer_type_object);
}

static void DestroyOptionValue(apOptionValue* option)
{
    free((void*)option->name);
    free((void*)option->edit);
    free((void*)option->desc);
    free((void*)option->display);
    if (option->type == OVT_STRING)
        free((void*)option->value.string);
    free((void*)option);
}

static apOptionValue* ParseOptionItem(cJSON* item)
{
    if (!cJSON_IsObject(item) || cJSON_IsInvalid(item))
        return 0;

    apOptionValue* option = (apOptionValue*)malloc(sizeof(apOptionValue));
    memset(option, 0, sizeof(*option));

    const char* name = GetString(item, "name", 0);
    if (!name)
        return 0;

    option->name = strdup(name);

    const char* edit = GetString(item, "edit", 0);
    option->edit = edit ? strdup(edit) : 0;

    cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
    if (cJSON_IsBool(value))
    {
        option->value.number = cJSON_IsTrue(value);
        option->type = OVT_BOOL;
    }
    else if (cJSON_IsNumber(value))
    {
        option->value.number = cJSON_GetNumberValue(value);
        option->type = OVT_NUMBER;
    }
    else if (cJSON_IsString(value))
    {
        option->value.string = strdup(cJSON_GetStringValue(value));
        option->type = OVT_STRING;
    }
    else
    {
        fprintf(stderr, "Property {name = '%s'} has mismatching value type (%d)", name, value->type);
        DestroyOptionValue(option);
        option = 0;
    }

    return option;
}

static void ParseExporter(apProject* p, cJSON* exporter)
{
    p->exporter = GetString(exporter, "exporter", 0);
    p->exporter = p->exporter ? strdup(p->exporter) : 0;

    // Keep them serialized for the presentation in the gui
    cJSON* options = cJSON_GetObjectItemCaseSensitive(exporter, "options");
    apOptionValue* last = 0;
    cJSON* item;
    cJSON_ArrayForEach(item, options)
    {
        apOptionValue* next = ParseOptionItem(item);

        if (next)
        {
            if (!p->exporter_options)
                p->exporter_options = next;
            else
                last->next = next;
            last = next;
        }
    }
}

static void ParseProjectFromJson(apProject* p, cJSON* json)
{
	ParseSources(p, cJSON_GetObjectItemCaseSensitive(json, "sources"));
	ParsePacker(p, cJSON_GetObjectItemCaseSensitive(json, "packer"));
    ParseExporter(p, cJSON_GetObjectItemCaseSensitive(json, "exporter"));
}

apProject* apLoadProjectFromMemory(const char* path, void* data)
{
	cJSON* json = 0;
	if (data)
	{
		json = cJSON_Parse((const char*)data);
		if (!json)
		{
			const char *error_ptr = cJSON_GetErrorPtr();
	        if (error_ptr != NULL)
	        {
	            fprintf(stderr, "Parse error before: '%s'\n", error_ptr);
	        }
			return 0;
		}
	}

	apProject* p = (apProject*)malloc(sizeof(apProject));
	memset(p, 0, sizeof(*p));

    apSetDefaultOptions(&p->options);
    apTilePackerSetDefaultOptions(&p->options_tp);
    apBinPackerSetDefaultOptions(&p->options_bp);

    p->packer_type = PT_TILEPACKER;

    if (json)
    {
    	ParseProjectFromJson(p, json);
    	cJSON_Delete(json);
    }

	return p;
}


apProject* apLoadProjectFromPath(const char* path)
{
	uint32_t file_size = 0;
	const char* data = (const char*)ReadFile(path, &file_size);
	if (!data)
		return 0;

	apProject* project = apLoadProjectFromMemory(path, (void*)data);
	free((void*)data);
	return project;
}

void apDestroyOptions(apOptionValue* option)
{
    while (option)
    {
        apOptionValue* next = option->next;
        DestroyOptionValue(option);
        option = next;
    }
}

void apDestroyProject(apProject* project)
{
    if (project->context)
        apDestroy(project->context);
    if (project->packer_type == PT_TILEPACKER && project->packer)
        apTilePackerDestroy(project->packer);
    if (project->packer_type == PT_BINPACKER && project->packer)
        apBinPackerDestroy(project->packer);
    apDestroyOptions(project->exporter_options);
    apDestroyOptions(project->exporter_defaults);
    free((void*)project->exporter);
    free((void*)project);
}

static void SaveGeneralPackerOptions(apProject* project, cJSON* packer_options)
{
    cJSON_AddNumberToObject(packer_options, "page_size", project->options.page_size);
}

static void SaveTilePackerOptions(apProject* project, cJSON* packer_options)
{
    cJSON_AddNumberToObject(packer_options, "no_rotate", project->options_tp.no_rotate);
    cJSON_AddNumberToObject(packer_options, "tile_size", project->options_tp.tile_size);
    cJSON_AddNumberToObject(packer_options, "padding", project->options_tp.padding);
    cJSON_AddNumberToObject(packer_options, "alpha_threshold", project->options_tp.alpha_threshold);
}

static void SaveBinPackerOptions(apProject* project, cJSON* packer_options)
{
    cJSON_AddNumberToObject(packer_options, "mode", project->options_bp.mode);
    cJSON_AddNumberToObject(packer_options, "no_rotate", project->options_bp.no_rotate);
}

static void SaveExporterOptions(apProject* project, cJSON* options)
{
    apOptionValue* option = project->exporter_options;
    while (option)
    {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", option->name);
        switch (option->type)
        {
        case OVT_BOOL:  cJSON_AddBoolToObject(item, "value", option->value.number != 0); break;
        case OVT_NUMBER:cJSON_AddNumberToObject(item, "value", option->value.number); break;
        case OVT_STRING:cJSON_AddStringToObject(item, "value", option->value.string); break;
        }
        cJSON_AddItemToArray(options, item);
        option = option->next;
    }
}

int apSaveProject(const char* path, apProject* project)
{
	cJSON* doc = cJSON_CreateObject();
    cJSON_AddNumberToObject(doc, "version", 1);

    cJSON* sources = cJSON_AddArrayToObject(doc, "sources");
    for (int i = 0; i < project->num_sources; ++i)
    {
        cJSON_AddItemToArray(sources, cJSON_CreateString(project->sources[i]));
    }

    cJSON* packer = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, "packer", packer);

    // general options
    cJSON* options = cJSON_CreateObject();
    cJSON_AddItemToObject(packer, "options", options);
    SaveGeneralPackerOptions(project, options);

    cJSON* packer_options = cJSON_CreateObject();

    if (project->packer_type == PT_BINPACKER)
    {
        cJSON_AddStringToObject(packer, "type", "binpacker");
        cJSON_AddItemToObject(packer, "binpacker", packer_options);

        SaveBinPackerOptions(project, packer_options);
    }
    else
    {
        cJSON_AddStringToObject(packer, "type", "tilepacker");
        cJSON_AddItemToObject(packer, "tilepacker", packer_options);

        SaveTilePackerOptions(project, packer_options);
    }

    if (project->exporter)
    {
        cJSON* exporter = cJSON_CreateObject();
        cJSON_AddItemToObject(doc, "exporter", exporter);

        cJSON_AddStringToObject(exporter, "exporter", project->exporter);

        if (project->exporter_options)
        {
            cJSON* options = cJSON_AddArrayToObject(exporter, "options");
            SaveExporterOptions(project, options);
        }
    }

    char* json_str = cJSON_Print(doc);

    FILE* file = fopen(path, "wb");
    if (!file)
    {
        fprintf(stderr, "Failed to open '%s' for writing", path);
        cJSON_free(json_str);
        cJSON_Delete(doc);
        return 0;
    }
    fwrite(json_str, strlen(json_str), 1, file);
    fclose(file);

    cJSON_free(json_str);
    cJSON_Delete(doc);
    return 1;
}

static void apProjectAddSource(apProject* project, const char* source)
{
	for (int i = 0; i < project->num_sources; ++i)
	{
		if (strcmp(project->sources[i], source) == 0)
			return; // It already existed
	}

	project->sources = (const char**)realloc(project->sources, (project->num_sources+1) * sizeof(const char*));
	project->sources[project->num_sources++] = strdup(source);
}

void apProjectAddSources(apProject* project, const char** sources, int num_sources)
{
	for (int i = 0; i < num_sources; ++i)
	{
		apProjectAddSource(project, sources[i]);
	}
}


void apDebugPrintProject(apProject* p)
{
	printf("Project\n");
	printf("  sources:\n");
	for (int i = 0; i < p->num_sources; ++i)
	{
		printf("    %d: '%s'\n", i, p->sources[i]);
	}
	printf("  \n");

	printf("  packer:\n");
	printf("    page_size: %d\n", p->options.page_size);

    printf("    type: %d\n", p->packer_type);

	if (p->packer_type == PT_TILEPACKER)
	{
		printf("    no_rotate: %d\n", p->options_tp.no_rotate);
		printf("    tile_size: %d\n", p->options_tp.tile_size);
		printf("    padding: %d\n", p->options_tp.padding);
		printf("    alpha_threshold: %d\n", p->options_tp.alpha_threshold);
	}
	else if (p->packer_type == PT_BINPACKER)
	{
		printf("    no_rotate: %d\n", p->options_bp.no_rotate);
		printf("    mode: %d\n", p->options_bp.mode);
	}

    if (p->exporter)
    {
        printf("  exporter:\n");
        printf("    exporter: %s\n", p->exporter);
        if (p->exporter_options)
        {
            printf("    options:\n");
            apOptionValue* option = p->exporter_options;
            while (option)
            {
                switch(option->type)
                {
                case OVT_BOOL:      printf("      '%s': '%s'\n", option->name, option->value.number != 0 ? "true":"false"); break;
                case OVT_NUMBER:    printf("      '%s': %f\n", option->name, option->value.number); break;
                case OVT_STRING:    printf("      '%s': %s\n", option->name, option->value.string); break;
                }

                option = option->next;
            }
        }
    }

	printf("  \n");
}
