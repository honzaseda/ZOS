//
// Created by Seda on 4. 1. 2017.
//

#ifndef ZOS_FAT_H
#define ZOS_FAT_H

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <cmath>
#include <vector>
#include <string.h>

class fat {
public:


    const int32_t FAT_UNUSED = INT32_MAX - 1; //2147483646
    const int32_t FAT_FILE_END = INT32_MAX - 2; //2147483645
    const int32_t FAT_BAD_CLUSTER = INT32_MAX - 3; //2147483644
    const int32_t FAT_DIRECTORY = INT32_MAX - 4; //2147483643

    fat();
    ~fat();

    struct boot_record {
        char volume_descriptor[250];    //popis vygenerovaného FS
        int8_t fat_type;                //typ FAT (FAT12, FAT16...) 2 na fat_type - 1 clusterů
        int8_t fat_copies;              //počet kopií FAT tabulek
        int16_t cluster_size;           //velikost clusteru
        int32_t usable_cluster_count;   //max počet clusterů, který lze použít pro data (-konstanty)
        char signature[9];              //podpis FS
    };

    struct directory {
        char name[13];                  //jméno souboru, nebo adresáře ve tvaru 8.3'/0' 12 + 1
        bool is_file;                    //identifikace zda je soubor (TRUE), nebo adresář (FALSE)
        int32_t size;                   //velikost položky, u adresáře 0
        int32_t start_cluster;          //počáteční cluster položky
    };

    struct directory_info {
        struct directory *dir;
        int32_t parent_dir;
    };

    FILE *fs;
    std::vector<directory_info> dir_info = std::vector<directory_info>();
    struct boot_record *fs_br;
    int32_t *fat_table;

    int fat_loader(char *name);

    void print_file_clusters(std::string file_path);

    void get_cluster_content(int32_t cluster);

    void get_file_content(std::string filepath);

    std::vector<std::string> explode(const std::string& str, const char& ch);

    int32_t get_file_start(std::vector<std::string> file_path);

    void print_tree();

    void print_directory(int iteration_level, int32_t curr_dir);

    int fat_creator(FILE *fp);
};


#endif //ZOS_FAT_H
