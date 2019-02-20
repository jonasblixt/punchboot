#include <pb/pb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>

#define INI_IMPLEMENTATION
#include "3pp/ini.h"

#include "pbimage.h"

static void pbimage_print_help(void) 
{
    printf ("pbimage:\n\n");
    printf ("  pbimage <config file>\n");
}

int main (int argc, char **argv) 
{
    void *memctx = NULL;
    int size = -1;
    char *data = NULL;
    uint32_t err;

    printf ("pbimage version %s\n\n", VERSION);

    if (argc <= 1) 
    {
        pbimage_print_help();
        exit(0);
    }

    
    /* Load configuration file */
    FILE* fp = fopen(argv[1], "r" );
    
    if (fp == NULL)
    {
        printf ("Error: could not read %s\n",argv[1]);
        return -1;
    }

    fseek( fp, 0, SEEK_END );
    size = ftell( fp );
    fseek( fp, 0, SEEK_SET );
    data = (char*) malloc( size + 1 );
    size_t read_sz = fread( data, 1, size, fp );
    data[ size ] = '\0';
    fclose( fp );

    if (read_sz != size)
    {
        printf ("Error: could not read file\n");
        return -1;
    }
    ini_t* ini = ini_load(data, memctx);
    free( data );

    /* Parse config file */
    uint32_t n = 0;
    uint32_t component_count = 0;
    uint32_t key_index = (uint32_t )-1;
    uint32_t key_mask = 0;
    uint32_t error_count = 0;
    const char *key_source = NULL;
    const char *section_name = NULL;
    const char *value = NULL;
    const char *output_fn = NULL;
    int index = -1;

    do
    {
        section_name = ini_section_name(ini, n);
        
        if (section_name)
        {

            if (strcmp(section_name,"pbimage") == 0)
            {

                index = ini_find_property(ini,n,"key_mask",0);

                if (index >= 0)
                {
                    value = ini_property_value(ini,n,index);
                    key_mask = strtol(value,NULL,16);
                }
                else
                {
                    printf ("Error: Could not read key mask\n");
                    error_count++;
                }

                index = ini_find_property(ini,n,"key_index",0);

                if (index >= 0)
                {
                    value = ini_property_value(ini,n,index);
                    key_index = atoi(value);
                }
                else
                {
                    printf ("Error: Could not read key index\n");
                    error_count++;
                }


                index = ini_find_property(ini,n,"key_source",0);

                if (index >= 0)
                {
                    key_source = ini_property_value(ini,n,index);
                }
                else
                {
                    printf ("Error: Could not read key source\n");
                    error_count++;
                }

                index = ini_find_property(ini,n,"output",0);

                if (index >= 0)
                {
                    output_fn = ini_property_value(ini,n,index);
                }
                else
                {
                    printf ("Error: Could not read output filename\n");
                    error_count++;
                }

            }
        }
        n++;

    } while (section_name);

    printf ("Punchboot image: \n");
    printf (" key index %u\n", key_index);
    printf (" key mask 0x%8.8X\n", key_mask);
    printf (" key source '%s'\n", key_source);
    printf (" output file '%s'\n", output_fn);

    if (error_count)
    {
        printf ("%u errors reading config file, aborting\n",error_count);
        return -1;
    }

    err = pbimage_prepare(key_index, key_mask, key_source, output_fn);
    
    if (err != PB_OK)
    {
        printf ("Could not create image, error %u\n", err);
        return -1;
    }

    n = 0;
    error_count = 0;
    const char *comp_type;
    const char *comp_file_fn;
    uint32_t comp_load_addr;
    
    do
    {
        section_name = ini_section_name(ini, n);
        
        if (section_name)
        {

            if (strcmp(section_name,"component") == 0)
            {
                error_count = 0;

                index = ini_find_property(ini,n,"type",0);

                if (index >= 0)
                {
                    comp_type = ini_property_value(ini,n,index);
                    component_count++;
                }
                else
                {
                    printf ("Error: Could not read component type\n");
                    error_count++;
                }

                index = ini_find_property(ini,n,"load_addr",0);

                if (index >= 0)
                {
                    value = ini_property_value(ini,n,index);

                    if (value)
                    {
                        comp_load_addr = strtol(value,NULL,16);
                    }
                    else
                    {
                        error_count++;
                    }  
                }
                else
                {
                    printf ("Error: Could not read component address\n");
                    error_count++;
                }

                index = ini_find_property(ini,n,"file",0);

                if (index >= 0)
                {
                    comp_file_fn = ini_property_value(ini,n,index);
                }
                else
                {
                    printf ("Error: Could not read component file name\n");
                    error_count++;
                }

                if (error_count == 0)
                {
                    printf ("Component %u\n",component_count-1);
                    printf (" type '%s'\n", comp_type);
                    printf (" load address 0x%8.8X\n", comp_load_addr);
                    printf (" source file '%s'\n",comp_file_fn);

                    err = pbimage_append_component(comp_type,
                                                   comp_load_addr, 
                                                   comp_file_fn);

                    if (err != PB_OK)
                    {
                        printf ("Error: could not append component (%u)\n",err);
                        return err;
                    }
                } 
                else
                {
                    printf ("Error could not read component information\n");
                    return -1;
                }
            }
        }       
        n++;

    } while (section_name);

    printf ("Generating output...\n");
    err = pbimage_out(output_fn);

    if (err != PB_OK)
    {
        printf ("Error: Signing failed (%u)\n",err);
    }
    ini_destroy( ini );

    return err;
}
