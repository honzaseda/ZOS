//
// Created by Seda on 4. 1. 2017.
//

#include <stack>
#include <cassert>
#include "fat.h"

fat::fat() {

}

fat::~fat() {
    fclose(fs);
}

/**
 * Loads a file system, stores boot record and FAT table informations
 * @param name Name of the FAT file
 * @return
 */
int fat::fat_loader(char *name) {
    fs_br = (struct boot_record *) malloc(sizeof(struct boot_record));

    if ((fs = fopen(name, "r+")) == NULL) {
        fs = fopen(name, "w+");
        fat_creator(fs);
    }
    fseek(fs, SEEK_SET, 0);
    fread(fs_br, sizeof(struct boot_record), 1, fs);
    /*
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "BOOT RECORD" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "volume_descriptor: " << fs_br->volume_descriptor << std::endl;
//    std::cout << "fat_type: " << fs_br->fat_type << std::endl;
    printf("fat_type: %d\n", fs_br->fat_type);
    printf("fat_copies: %d\n", fs_br->fat_copies);
//    std::cout << "fat_copies: " << fs_br->fat_copies << std::endl;
    std::cout << "cluster_size: " << fs_br->cluster_size << std::endl;
    std::cout << "usable_cluster_count: " << fs_br->usable_cluster_count << std::endl;
    std::cout << "signature: " << fs_br->signature << std::endl;

    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "FAT table" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    */
    fat_table = (int32_t *) malloc(sizeof(int32_t) * fs_br->usable_cluster_count);
    for (int l = 0; l < fs_br->fat_copies; l++) {
        fread(fat_table, sizeof(*fat_table) * fs_br->usable_cluster_count, 1, fs);
    }
/*
    for (int asdf = 0; asdf < fs_br->usable_cluster_count; asdf++) {
        if (fat_table[asdf] != FAT_UNUSED) {
            printf("[%d] %d\n", asdf, fat_table[asdf]);
        }
    }
    */
    set_cluster_data();
    return 0;
}

/**
 * Stores FAT data (file information, cluster content) into an appropriate vectors
 */
void fat::set_cluster_data() {
    int32_t clust_pos = (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count));
    fseek(fs, clust_pos, SEEK_SET);
    char *cluster_content = (char *) malloc(fs_br->cluster_size * sizeof(char) + sizeof(char));
    memset(cluster_content, '\0', fs_br->cluster_size + 1);
    long max_cluster_dirs = fs_br->cluster_size / sizeof(struct directory);
    for (int i = 0; i < fs_br->usable_cluster_count; i++) {
        if (fat_table[i] == FAT_DIRECTORY) {
            for (int j = 0; j < max_cluster_dirs; j++) {
                struct directory *fs_dir;
                fs_dir = (struct directory *) malloc(sizeof(struct directory));

                fread(fs_dir, sizeof(struct directory), 1, fs);
                if (fs_dir->start_cluster != 0) {
                    struct directory_info temp_info;
                    temp_info.dir = fs_dir;
                    temp_info.parent_dir = i;
                    dir_info.push_back(temp_info);
//                    std::cout << "FILE " << j << std::endl;
//                    std::cout << "name: " << fs_dir->name << std::endl;
//                    std::cout << "file_type: " << fs_dir->is_file << std::endl;
//                    std::cout << "file_size: " << fs_dir->size << std::endl;
//                    std::cout << "first_cluster: " << fs_dir->start_cluster << std::endl;
                }
            }
            cluster_data.push_back("DIR");
            fseek(fs, fs_br->cluster_size - max_cluster_dirs * sizeof(struct directory), SEEK_CUR);
        } else {
            fread(cluster_content, (size_t) fs_br->cluster_size, 1, fs);
            cluster_data.push_back(cluster_content);
        }
    }
    free(cluster_content);
}

/**
 * Adding a content of an external file into a specified path in the FAT file system
 * @param file_path Name of the input file
 * @param fat_path FAT path where the input file content will be located
 */
void fat::create_new_file(std::string file_path, std::string fat_path) {
    FILE *fnew;
    std::vector<std::string> result = explode(fat_path, '/');

    struct directory root;
    std::vector<std::string> split_content = std::vector<std::string>();
    std::vector<int32_t> free_clusters = std::vector<int32_t>();

    // ---
    // Reading the input file and storing its content into a string vector
    // ---
    fnew = fopen(file_path.c_str(), "r");
    fseek(fnew, 0, SEEK_END);
    long fsize = ftell(fnew);
    fseek(fnew, 0, SEEK_SET);
    std::string content(fsize, '\0');
    fread(&content[0], fsize, 1, fnew);
    fclose(fnew);
    int NumSubstrings = content.length() / fs_br->cluster_size;
    for (auto i = 0; i < NumSubstrings; i++) {
        split_content.push_back(content.substr(i * fs_br->cluster_size, fs_br->cluster_size));
    }
    if (content.length() % fs_br->cluster_size != 0) {
        split_content.push_back(content.substr(fs_br->cluster_size * NumSubstrings));
    }

    for (int i = 0; i < split_content.size(); i++) {
        free_clusters.push_back(get_first_free_cluster());
    }
    assert(free_clusters.size() == split_content.size());

    // ---
    // Updating the parent directory content
    // ---
    memset(root.name, '\0', sizeof(root.name));
    root.is_file = 1;
    strcpy(root.name, file_path.c_str());
    root.size = fsize;
    root.start_cluster = free_clusters.at(0);

    int32_t parent = get_parent_cluster(result);
    if (parent < 0) {
        std::cout << "PATH NOT FOUND" << std::endl;
        exit(1);
    }
    std::vector<fat::directory> children = get_dir_children(parent);
    fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
               parent * fs_br->cluster_size), SEEK_SET);
    int16_t ac_size = 0;
    for (int j = 0; j < children.size(); j++) {
        fwrite(&children.at(j), sizeof(directory), 1, fs);
        ac_size += sizeof(directory);
    }
    fwrite(&root, sizeof(root), 1, fs);
    ac_size += sizeof(directory);
    char buffer[] = {'\0'};
    for (int16_t i = 0; i < (fs_br->cluster_size - ac_size); i++) {
        fwrite(buffer, sizeof(buffer), 1, fs);
    }

    // ---
    // Updating all FAT table copies
    // ---
    int32_t new_fat[fs_br->usable_cluster_count];
    for (int k = 0; k < fs_br->usable_cluster_count; k++) {
        new_fat[k] = fat_table[k];
    }
    for (int x = 0; x < free_clusters.size(); x++) {
        if (x != free_clusters.size() - 1) {
            new_fat[free_clusters.at(x)] = free_clusters.at(x + 1);
        } else {
            new_fat[free_clusters.at(x)] = FAT_FILE_END;
        }
    }
    fseek(fs, sizeof(boot_record), SEEK_SET);
    for (int i = 0; i < fs_br->fat_copies; i++) {
        fwrite(&new_fat, sizeof(new_fat), 1, fs);
    }

    // ---
    // Inserting the input file content into FAT clusters
    // ---
    for (int i = 0; i < free_clusters.size(); i++) {
        char cluster[fs_br->cluster_size];
        memset(cluster, '\0', sizeof(cluster));
        strcpy(cluster, split_content.at(i).c_str());

        fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count)),
              SEEK_SET);
        fseek(fs, fs_br->cluster_size * free_clusters.at(i), SEEK_CUR);
        fwrite(&cluster, sizeof(cluster), 1, fs);
    }
    std::cout << "OK" << std::endl;
}

/**
 * Get first empty cluster in FAT acquired from FAT table
 * @return Integer value of the empty cluster
 */
int32_t fat::get_first_free_cluster() {
    for (int i = 0; i < fs_br->usable_cluster_count; i++) {
        if (fat_table[i] == FAT_UNUSED) {
            fat_table[i] = FAT_FILE_END;
            return i;
        }
    }
    return 0;
}

/**
 * Obtaining all files that are children to the specified parent folder
 * @param dir_cluster Integer value of the parent directory cluster
 * @return Vector of all children
 */
std::vector<fat::directory> fat::get_dir_children(int32_t dir_cluster) {
    std::vector<directory> children = std::vector<directory>();
    for (int i = 0; i < dir_info.size(); i++) {
        if (dir_info.at(i).parent_dir == dir_cluster) {
            children.push_back(*dir_info.at(i).dir);
        }
    }
    return children;
}

/**
 * Getting the cluster of the most nested directory of the passed path
 * @param file_path Complete path in the FAT file system
 * @return Integer value of the cluster number, if not found, returns -1
 */
int32_t fat::get_parent_cluster(std::vector<std::string> file_path) {
    std::stack<int32_t> last_parent;
    last_parent.push(0);
    if (file_path.size() == 0) {
        return 0;
    } else {
        for (int i = 0; i < file_path.size(); i++) {
            for (int j = 0; j < dir_info.size(); j++) {
                if (strcmp(dir_info.at(j).dir->name, file_path[i].c_str()) == 0) {
                    if ((!dir_info.at(j).dir->is_file) && (i == (file_path.size() - 1))) {
                        if (dir_info.at(j).parent_dir == last_parent.top()) {
                            return dir_info.at(j).dir->start_cluster;
                        }
                    } else {
                        if (last_parent.top() == dir_info.at(j).parent_dir && !dir_info.at(j).dir->is_file) {
                            last_parent.push(dir_info.at(j).dir->start_cluster);
                        }
                    }
                }
            }

        }
    }
    return -1;
}

/**
 * Creates a new directory in the FAT file system and updates all FAT tables
 * @param dir_name New directory name
 * @param fat_path Path to the new directory in file system
 */
void fat::create_new_directory(std::string dir_name, std::string fat_path) {
    std::vector<std::string> result = explode(fat_path, '/');
    struct directory root;

    int32_t cluster = get_first_free_cluster();

    // ---
    // Updating the parent directory content
    // ---
    memset(root.name, '\0', sizeof(root.name));
    root.is_file = 0;
    strcpy(root.name, dir_name.c_str());
    root.size = 0;
    root.start_cluster = cluster;

    int32_t parent = get_parent_cluster(result);
    if (parent < 0) {
        std::cout << "PATH NOT FOUND" << std::endl;
        exit(1);
    }
    std::vector<fat::directory> children = get_dir_children(parent);
    fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
               parent * fs_br->cluster_size), SEEK_SET);
    int16_t ac_size = 0;
    for (int j = 0; j < children.size(); j++) {
        fwrite(&children.at(j), sizeof(fat::directory), 1, fs);
        ac_size += sizeof(fat::directory);
    }
    fwrite(&root, sizeof(root), 1, fs);
    ac_size += sizeof(directory);
    char buffer[] = {'\0'};
    for (int16_t i = 0; i < (fs_br->cluster_size - ac_size); i++) {
        fwrite(buffer, sizeof(buffer), 1, fs);
    }

    // ---
    // Updating all FAT table copies
    // ---
    int32_t new_fat[fs_br->usable_cluster_count];
    for (int k = 0; k < fs_br->usable_cluster_count; k++) {
        new_fat[k] = fat_table[k];
    }
    new_fat[cluster] = FAT_DIRECTORY;

    fseek(fs, sizeof(boot_record), SEEK_SET);
    for (int i = 0; i < fs_br->fat_copies; i++) {
        fwrite(&new_fat, sizeof(new_fat), 1, fs);
    }

    char cluster_empty[fs_br->cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
               cluster * fs_br->cluster_size), SEEK_SET);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fs);

    std::cout << "OK" << std::endl;
}

/**
 * Deletes an empty directory from FAT file system
 * @param dir_path Path to the directory
 */
void fat::delete_directory(std::string dir_path) {
    std::vector<std::string> result = explode(dir_path, '/');

    int32_t directory = get_parent_cluster(result);
    std::vector<fat::directory> dir_children = get_dir_children(directory);
    if(dir_children.size() > 0){
        std::cout << "PATH NOT EMPTY" << std::endl;
        exit(1);
    }

    if (directory < 0) {
        std::cout << "PATH NOT FOUND" << std::endl;
        exit(1);
    }

    result.pop_back();
    int32_t parent = get_parent_cluster(result);
    std::vector<fat::directory> parent_children = get_dir_children(parent);
    // ---
    // Updating the parent directory content
    // ---
    fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
               parent * fs_br->cluster_size), SEEK_SET);
    int16_t ac_size = 0;
    for (int j = 0; j < parent_children.size(); j++) {
        if(parent_children.at(j).start_cluster != directory) {
            fwrite(&parent_children.at(j), sizeof(fat::directory), 1, fs);
            ac_size += sizeof(fat::directory);
        }
    }
    char buffer[] = {'\0'};
    for (int16_t i = 0; i < (fs_br->cluster_size - ac_size); i++) {
        fwrite(buffer, sizeof(buffer), 1, fs);
    }

    // ---
    // Updating all FAT table copies
    // ---
    int32_t new_fat[fs_br->usable_cluster_count];
    for (int k = 0; k < fs_br->usable_cluster_count; k++) {
        new_fat[k] = fat_table[k];
    }
    new_fat[directory] = FAT_UNUSED;

    fseek(fs, sizeof(boot_record), SEEK_SET);
    for (int i = 0; i < fs_br->fat_copies; i++) {
        fwrite(&new_fat, sizeof(new_fat), 1, fs);
    }

    // ---
    // Erasing directory 'content' in FAT, just to be sure
    // ---
    char cluster_empty[fs_br->cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
               directory * fs_br->cluster_size), SEEK_SET);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fs);
}

/**
 * Prints out numbers of all the clusters that the file is stored on
 * @param file_path Complete path to the file in the FAT file system
 */
void fat::print_file_clusters(std::string file_path) {
    std::vector<std::string> result = explode(file_path, '/');
    int32_t found_file = get_file_start(result);
    if (found_file != 0) {
        std::cout << file_path << ": " << found_file;
        int32_t curr_cluster = found_file;
        while (fat_table[curr_cluster] != FAT_FILE_END) {
            std::cout << ", " << fat_table[curr_cluster];
            curr_cluster = fat_table[curr_cluster];
        }
    } else {
        std::cout << "PATH NOT FOUND" << std::endl;
    }
}

/**
 * Prints out content of the cluster
 * @param Cluster number of cluster
 */
void fat::get_cluster_content(int32_t cluster) {
    //pokud je prazdny (tedy zacina 0, tak nevypisuj obsah)
    if (fat_table[cluster] == FAT_FILE_END) {
        std::cout << cluster_data.at(cluster) << std::endl;
    } else {
        std::cout << cluster_data.at(cluster);
        get_cluster_content(fat_table[cluster]);
    }
}

/**
 * Recursively calls function to print out the content of a file, specified by path, stored in file system
 * @param file_path A path to the file, stored in the file system
 */
void fat::get_file_content(std::string file_path) {
    std::vector<std::string> result = explode(file_path, '/');
    int32_t found_file = get_file_start(result);
    if (found_file != 0) {
        std::cout << file_path << ": ";
        get_cluster_content(found_file);
    } else {
        std::cout << "PATH NOT FOUND" << std::endl;
    }
}

/**
 * Function for returning a starting cluster of a file, specified by file path
 * @param file_path Full FAT path to the file
 * @return Integer value of the starting cluster
 */
int32_t fat::get_file_start(std::vector<std::string> file_path) {
    std::stack<int32_t> last_parent;
    last_parent.push(0);
    for (int i = 0; i < file_path.size(); i++) {
        for (int j = 0; j < dir_info.size(); j++) {
            if (strcmp(dir_info.at(j).dir->name, file_path[i].c_str()) == 0) {
                if ((dir_info.at(j).dir->is_file) && (i == (file_path.size() - 1))) {
                    if (dir_info.at(j).parent_dir == last_parent.top()) {
                        return dir_info.at(j).dir->start_cluster;
                    }
                } else {
                    if (last_parent.top() == dir_info.at(j).parent_dir) {
                        last_parent.push(dir_info.at(j).dir->start_cluster);
                        break;
                    }
                }
            }
        }

    }
    return 0;
}

/**
 * Splits a string by a delimiter
 * @param str String to be splitted
 * @param ch Delimiter character
 * @return Vector of all splitted strings
 */
std::vector<std::string> fat::explode(const std::string &str, const char &ch) {
    std::string next;
    std::vector<std::string> result;
    for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
        // If we've hit the terminal character
        if (*it == ch) {
            // If we have some characters accumulated
            if (!next.empty()) {
                // Add them to the result vector
                result.push_back(next);
                next.clear();
            }
        } else {
            // Accumulate the next character into the sequence
            next += *it;
        }
    }
    if (!next.empty())
        result.push_back(next);
    return result;
}

/**
 * Checks if the file system is not empty and calls a function to print out the file system tree structure
 */
void fat::print_tree() {
    bool rootEmpty = true;
    for (int i = 0; i < dir_info.size(); i++) {
        if (dir_info.at(i).parent_dir == 0) {
            rootEmpty = false;
            break;
        }
    }
    if (!rootEmpty) {
        std::cout << "+ROOT" << std::endl;
        print_directory(1, 0);
        std::cout << "--" << std::endl;
    } else {
        std::cout << "EMPTY" << std::endl;
    }
}

/**
 * Recursively prints out all files or directories contained in a current directory
 * @param iteration_level Directory nest level
 * @param curr_dir Current directory
 */
void fat::print_directory(int iteration_level, int32_t curr_dir) {
    std::string indent = "";
    for (int i = 0; i < iteration_level; i++) {
        indent.append("\t");
    }
    for (int j = 0; j < dir_info.size(); j++) {
        if (dir_info.at(j).parent_dir == curr_dir) {
            if (dir_info.at(j).dir->is_file) {

                std::cout << indent << "-" << dir_info.at(j).dir->name << " " << dir_info.at(j).dir->start_cluster
                          << " " << get_cluster_count(dir_info.at(j).dir->start_cluster)
                          << std::endl;
            } else {
                std::cout << indent << "+" << dir_info.at(j).dir->name << std::endl;
                print_directory(iteration_level + 1, dir_info.at(j).dir->start_cluster);
                std::cout << indent << "--" << std::endl;
            }
        }
    }
}

/**
 * Returns a number of clusters that a file is stored on
 * @param fat_start Starting cluster of a file
 * @return Number of clusters
 */
int32_t fat::get_cluster_count(int32_t fat_start) {
    int32_t count = 1;
    while (fat_table[fat_start] != FAT_FILE_END) {
        count++;
        fat_start = fat_table[fat_start];
    }
    return count;
}

/**
 *
 * @param fp
 * @return
 */
int fat::fat_creator(FILE *fp) {
    struct boot_record br;
    struct directory root_a, root_b, root_c, root_d, root_e, root_f, root_g;

    memset(br.volume_descriptor, '\0', sizeof(br.volume_descriptor));
    memset(br.signature, '\0', sizeof(br.signature));
    char volume_descriptor[250] = "Testovaci fat 5 - type: 8, copies: 2, cluster_size: 256, usable_cluster: 251";
    strcpy(br.volume_descriptor, volume_descriptor);
    strcpy(br.signature, "Seda");
    br.fat_type = 8;
    br.fat_copies = 2;
    br.cluster_size = 256;
    br.usable_cluster_count = 251;

    //directory - vytvoreni polozek
    memset(root_a.name, '\0', sizeof(root_a.name));
    root_a.is_file = 1;
    strcpy(root_a.name, "cisla.txt");
    root_a.size = 135;
    root_a.start_cluster = 1;

    memset(root_b.name, '\0', sizeof(root_b.name));
    root_b.is_file = 1;
    strcpy(root_b.name, "pohadka.txt");
    root_b.size = 5975;
    root_b.start_cluster = 2;

    memset(root_c.name, '\0', sizeof(root_c.name));
    root_c.is_file = 1;
    strcpy(root_c.name, "msg.txt");
    root_c.size = 396;
    root_c.start_cluster = 30;

    memset(root_d.name, '\0', sizeof(root_d.name));
    root_d.is_file = 0;
    strcpy(root_d.name, "direct-1");
    root_d.size = 0;
    root_d.start_cluster = 29;

    memset(root_e.name, '\0', sizeof(root_e.name));
    root_e.is_file = 0;
    strcpy(root_e.name, "podslozka");
    root_e.size = 0;
    root_e.start_cluster = 34;

    memset(root_f.name, '\0', sizeof(root_f.name));
    root_f.is_file = 1;
    strcpy(root_f.name, "cisla.txt");
    root_f.size = 139;
    root_f.start_cluster = 35;

    memset(root_g.name, '\0', sizeof(root_g.name));
    root_g.is_file = 0;
    strcpy(root_g.name, "cosituje");
    root_g.size = 0;
    root_g.start_cluster = 36;

    // pro zapis budu potrebovat i prazdny cluster
    char cluster_empty[br.cluster_size];
    char cluster_empty_dir[br.cluster_size];
    ////////////////////////////////////////////// SOUBORY A ADRESARE POKUSNE
    char cluster_dir1[br.cluster_size];
    char cluster_slozka[br.cluster_size];
    char cluster_a[br.cluster_size];
    char cluster_b1[br.cluster_size];
    char cluster_b2[br.cluster_size];
    char cluster_b3[br.cluster_size];
    char cluster_b4[br.cluster_size];
    char cluster_b5[br.cluster_size];
    char cluster_b6[br.cluster_size];
    char cluster_b7[br.cluster_size];
    char cluster_b8[br.cluster_size];
    char cluster_b9[br.cluster_size];
    char cluster_b10[br.cluster_size];
    char cluster_b11[br.cluster_size];
    char cluster_b12[br.cluster_size];
    char cluster_b13[br.cluster_size];
    char cluster_b14[br.cluster_size];
    char cluster_b15[br.cluster_size];
    char cluster_b16[br.cluster_size];
    char cluster_b17[br.cluster_size];
    char cluster_b18[br.cluster_size];
    char cluster_b19[br.cluster_size];
    char cluster_b20[br.cluster_size];
    char cluster_b21[br.cluster_size];
    char cluster_b22[br.cluster_size];
    char cluster_b23[br.cluster_size];
    char cluster_b24[br.cluster_size];
    char cluster_c1[br.cluster_size];
    char cluster_c2[br.cluster_size];
    char cluster_f1[br.cluster_size];

    //pripravim si obsah - delka stringu musi byt stejna jako velikost clusteru
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    memset(cluster_a, '\0', sizeof(cluster_a));
    memset(cluster_b1, '\0', sizeof(cluster_b1));
    memset(cluster_b2, '\0', sizeof(cluster_b2));
    memset(cluster_b3, '\0', sizeof(cluster_b3));
    memset(cluster_b4, '\0', sizeof(cluster_b4));
    memset(cluster_b5, '\0', sizeof(cluster_b5));
    memset(cluster_b6, '\0', sizeof(cluster_b6));
    memset(cluster_b7, '\0', sizeof(cluster_b7));
    memset(cluster_b8, '\0', sizeof(cluster_b8));
    memset(cluster_b9, '\0', sizeof(cluster_b9));
    memset(cluster_b10, '\0', sizeof(cluster_b10));
    memset(cluster_b11, '\0', sizeof(cluster_b11));
    memset(cluster_b12, '\0', sizeof(cluster_b12));
    memset(cluster_b13, '\0', sizeof(cluster_b13));
    memset(cluster_b14, '\0', sizeof(cluster_b14));
    memset(cluster_b15, '\0', sizeof(cluster_b15));
    memset(cluster_b16, '\0', sizeof(cluster_b16));
    memset(cluster_b17, '\0', sizeof(cluster_b17));
    memset(cluster_b18, '\0', sizeof(cluster_b18));
    memset(cluster_b19, '\0', sizeof(cluster_b19));
    memset(cluster_b20, '\0', sizeof(cluster_b20));
    memset(cluster_b21, '\0', sizeof(cluster_b21));
    memset(cluster_b22, '\0', sizeof(cluster_b22));
    memset(cluster_b23, '\0', sizeof(cluster_b23));
    memset(cluster_b24, '\0', sizeof(cluster_b24));
    memset(cluster_c1, '\0', sizeof(cluster_c1));
    memset(cluster_c2, '\0', sizeof(cluster_c2));
    memset(cluster_f1, '\0', sizeof(cluster_f1));
    memset(cluster_dir1, '\0', sizeof(cluster_dir1));
    memset(cluster_slozka, '\0', sizeof(cluster_slozka));
    memset(cluster_slozka, '\0', sizeof(cluster_slozka));
    strcpy(cluster_a,
           "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789 - tohle je malicky soubor pro test");
    strcpy(cluster_b1,
           "Byla jednou jedna sladka divenka, kterou musel milovat kazdy, jen ji uvidel, ale nejvice ji milovala jeji babicka, ktera by ji snesla i modre z nebe. Jednou ji darovala cepecek karkulku z cerveneho sametu a ten se vnucce tak libil, ze nic jineho nechtela ");
    strcpy(cluster_b2,
           "nosit, a tak ji zacali rikat Cervena Karkulka. Jednou matka Cervene Karkulce rekla: ?Podivej, Karkulko, tady mas kousek kolace a lahev vina, zanes to babicce, je nemocna a zeslabla, timhle se posilni. Vydej se na cestu drive nez bude horko, jdi hezky spor");
    strcpy(cluster_b3,
           "adane a neodbihej z cesty, kdyz upadnes, lahev rozbijes a babicka nebude mit nic. A jak vejdes do svetnicky, nezapome?babicce poprat dobreho dne a ne abys smejdila po vsech koutech.? ?Ano, maminko, udelam, jak si prejete.? rekla Cerveni Karkulka, na stvrz");
    strcpy(cluster_b4,
           "eni toho slibu podala matce ruku a vydala se na cestu. Babicka bydlela v lese; celou p?lhodinu cesty od vesnice. Kdyz sla Cervena Karkulka lesem, potkala vlka. Tenkrat jeste nevedela, co je to za zaludne zvire a ani trochu se ho nebala. ?Dobry den, Cerven");
    strcpy(cluster_b5,
           "a Karkulko!? rekl vlk. ?Dekuji za prani, vlku.? ?Kampak tak casne, Cervena Karkulko?? ?K babicce!? ?A copak to neses v zasterce?? ?Kolac a vino; vcera jsme pekli, nemocne a zeslable babicce na posilnenou.? ?Kdepak bydli babicka, Cervena Karkulko?? ?Inu, j");
    strcpy(cluster_b6,
           "este tak ctvrthodiny cesty v lese, jeji chaloupka stoji mezi tremi velkymi duby, kolem je liskove oresi, urcite to tam musis znat.? odvetila Cervena Karkulka. Vlk si pomyslil: ?Tohle mla?ucke, jem?ucke masicko bude jiste chutnat lepe nez ta starena, mus");
    strcpy(cluster_b7,
           "im to navleci lstive, abych schlamstnul obe.? Chvili sel vedle Cervene Karkulky a pak pravil: ?Cervena Karkulko, koukej na ty krasne kvetiny, ktere tu rostou vsude kolem, procpak se trochu nerozhlednes? Myslim, ze jsi jeste neslysela ptacky, kteri by zpiv");
    strcpy(cluster_b8,
           "ali tak libezne. Ty jsi tu vykracujes, jako kdybys sla do skoly a pritom je tu v lese tak krasne!? Cervena Karkulka otevrela oci dokoran a kdyz videla, jak slunecni paprsky tancuji skrze stromy sem a tam a vsude roste tolik krasnych kvetin, pomyslila si: ");
    strcpy(cluster_b9,
           "?Kdyz prinesu babicce kytici cerstvych kvetin, bude mit jiste radost, casu mam dost, prijdu akorat.? A sebehla z cesty do lesa a trhala kvetiny. A kdyz jednu utrhla, zjistila, ze o kus dal roste jeste krasnejsi, bezela k ni, a tak se dostavala stale hloub");
    strcpy(cluster_b10,
           "eji do lesa. Ale vlk bezel rovnou k babiccine chaloupce a zaklepal na dvere. ?Kdo je tam?? ?Cervena Karkulka, co nese kolac a vino, otevri!? ?Jen zmackni kliku,? zavolala babicka: ?jsem prilis slaba a nemohu vstat.? Vlk vzal za kliku, otevrel dvere a beze");
    strcpy(cluster_b11,
           "slova sel rovnou k babicce a spolknul ji. Pak si obleknul jeji saty a nasadil jeji cepec, polozil se do postele a zatahnul zaves. Zatim Cervena Karkulka behala mezi kvetinami, a kdyz jich mela naruc tak plnou, ze jich vic nemohla pobrat, tu ji prisla na  ");
    strcpy(cluster_b12,
           "mysl babicka, a tak se vydala na cestu za ni. Podivila se, ze jsou dvere otevrene, a kdyz vesla do svetnice, prislo ji vse takove podivne, ze si pomyslila: ?Dobrotivy Boze, je mi dneska nejak ?zko a jindy jsem u babicky tak rada.? Zvolala: ?Dobre jitro!? ");
    strcpy(cluster_b13,
           "Ale nedostala zadnou odpove? ?la tedy k posteli a odtahla zaves; lezela tam babicka a mela cepec narazeny hluboko do obliceje a vypadala nejak podivne. Ach, babicko, proc mas tak velke usi?? ?Abych te lepe slysela.? ?Ach, babicko, proc mas tak velke oci ");
    strcpy(cluster_b14,
           "?? ?Abych te lepe videla.? ?Ach, babicko, proc mas tak velke ruce?? ?Abych te lepe objala.? ?Ach, babicko, proc mas tak straslivou tlamu?? ?Abych te lepe sezrala!!? Sotva vlk ta slova vyrknul, vyskocil z postele a ubohou Cervenou Karkulku spolknul. Kdyz t");
    strcpy(cluster_b15,
           "e?uhasil svoji zadostivost, polozil se zpatky do postele a usnul a z toho spanku se jal mocne chrapat. Zrovna sel kolem chaloupky lovec a pomyslil si: ?Ta starenka ale chrape, musim se na ni podivat, zda neco nepotrebuje.? Vesel do svetnice, a kdyz prist");
    strcpy(cluster_b16,
           "oupil k posteli, uvidel, ze v ni lezi vlk. ?Tak prece jsem te nasel, ty stary hrisniku!? zvolal lovec: ?Uz mam na tebe dlouho policeno!? Strhnul z ramene pusku, ale pak mu prislo na mysl, ze vlk mohl sezrat babicku a mohl by ji jeste zachranit. Nestrelil ");
    strcpy(cluster_b17,
           "tedy, nybrz vzal n?zky a zacal spicimu vlkovi parat bricho. Sotva ucinil par rez?, uvidel se cervenat karkulku a po par dalsich rezech vyskocila divenka ven a volala: ?Ach, ja jsem se tolik bala, ve vlkovi je cernocerna tma.? A potom vylez la ven i ziva b");
    strcpy(cluster_b18,
           "abicka; sotva dechu popadala. Cervena Karkulka pak nanosila obrovske kameny, kterymi vlkovo bricho naplnili, a kdyz se ten probudil a chtel uteci, kameny ho tak desive tizily, ze klesnul k zemi nadobro mrtvy. Ti tri byli spokojeni. Lovec stahnul vlkovi ko");
    strcpy(cluster_b19,
           "zesinu a odnesl si ji dom?, babicka snedla kolac a vypila vino, ktere Cervena Karkulka prinesla, a opet se zotavila. A Cervena Karkulka? Ta si svatosvate prisahala: ?Uz nikdy v zivote nesejdu z cesty do lesa, kdyz mi to maminka zakaze!? O Cervene  Karkulc");
    strcpy(cluster_b20,
           "e se jeste vypravi, ze kdyz sla jednou zase k babicce s babovkou, potkala jineho vlka a ten se ji taky vemlouval a snazil se ji svest z cesty. Ale  ona se toho vystrihala a kracela rovnou k babicce, kde hned vypovedela, ze potkala vlka, ktery ji sice popr");
    strcpy(cluster_b21,
           "al dobry den, ale z oci mu koukala nekalota. ?Kdyby to nebylo na verejne ceste, jiste by mne sezral!? ?Poj??  rekla babicka: ?zavreme dobre dvere, aby nemohl dovnitr.? Brzy nato zaklepal vlk a zavolal: ?Otevri, babicko, ja jsem Cervena Karkulka a nesu ti");
    strcpy(cluster_b22,
           "pecivo!? Ty dve vsak z?staly jako peny a neotevrely. Tak se ten sedivak plizil kolem domu a naslouchal, pak vylezl na strechu, aby tam pockal, az Cervena Karkulka p?jde vecer dom?, pak ji v temnote popadne a sezere. Ale babicka zle vlkovy ?mysly odhalila ");
    strcpy(cluster_b23,
           ". Pred domem staly obrovske kamenne necky, tak Cervene  Karkulce rekla: ?Vezmi vedro, devenko, vcera jsem varila klobasy, tak tu vodu nanosime venku do necek.? Kdyz byly necky plne, stoupala v?ne klobas nahoru az k vlkovu cenichu. Zavetril a natahoval krk");
    strcpy(cluster_b24,
           "tak daleko, ze se na strese vice neudrzel a zacal klouzat dol?, kde spadnul primo do necek a bidne se utopil.");
    strcpy(cluster_c1,
           "Prodej aktiv SABMilleru v Ceske republice, Polsku, Ma?rsku, Rumunsku a na Slovensku je soucasti podminek pro prevzeti podniku ze strany americkeho pivovaru Anheuser-Busch InBev, ktere bylo dokonceno v rijnu. Krome Plze?keho Prazdroje zahrnuji prodavana ");
    strcpy(cluster_c2,
           "aktiva polske znacky Tyskie a Lech, slovensky Topvar, ma?rsky Dreher a rumunsky Ursus. - Tento soubor je sice kratky, ale neni fragmentovany");
    strcpy(cluster_f1,
           "moje0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789 - tohle je malicky soubor pro test");

/////////// ZACATEK VYTVARENI FAT TABULKY

    int32_t fat[br.usable_cluster_count];

    //ve fatce na 0 clusteru bude root directory
    fat[0] = FAT_DIRECTORY;
    //pak bude soubor "cisla.txt" - ten je jen jednoclusterovy
    fat[1] = FAT_FILE_END;
    //pak bude dlouhy soubor "pohadka.txt", ktery je cely za sebou
    fat[2] = 3;
    fat[3] = 4;
    fat[4] = 5;
    fat[5] = 6;
    fat[6] = 7;
    fat[7] = 8;
    fat[8] = 9;
    fat[9] = 10;
    fat[10] = 11;
    fat[11] = 12;
    fat[12] = 13;
    fat[13] = 14;
    fat[14] = 15;
    fat[15] = 16;
    fat[16] = 17;
    fat[17] = 18;
    fat[18] = 19;
    fat[19] = 20;
    fat[20] = 21;
    fat[21] = 22;
    fat[22] = 23;
    fat[23] = 24;
    fat[24] = 25;
    fat[25] = FAT_FILE_END;
    //ted bude nejake volne misto
    fat[26] = FAT_UNUSED;
    fat[27] = FAT_UNUSED;
    fat[28] = FAT_UNUSED;
    //pak adresar "directory-1"
    fat[29] = FAT_DIRECTORY;
    //pak soubor "msg.txt"
    fat[30] = 33;
    //bohuzel pri jeho zapisu se se muselo fragmenotvat - jsou tu dva spatne sektory
    fat[31] = FAT_BAD_CLUSTER;
    fat[32] = FAT_BAD_CLUSTER;
    //a tady je konec "msg.txt"
    fat[33] = FAT_FILE_END;
    //podslozka
    fat[34] = FAT_DIRECTORY;
    fat[35] = FAT_FILE_END;
    fat[36] = FAT_DIRECTORY;
    //zbytek bude prazdny
    for (int32_t i = 37; i < br.usable_cluster_count; i++) {
        fat[i] = FAT_UNUSED;
    }

/////////// KONEC VYTVARENI FAT TABULKY

    //boot record
    fwrite(&br, sizeof(br), 1, fp);
    // 2x FAT
    fwrite(&fat, sizeof(fat), 1, fp);
    fwrite(&fat, sizeof(fat), 1, fp);
    //dir - bacha, tady je potreba zapsat CELY CLUSTER, ne jen prvnich n-BYTU plozek - tedy doplnit nulami poradi zaznamu v directory NEMUSI odpovidat poradi ve FATce a na disku
    int16_t cl_size = br.cluster_size;
    int16_t ac_size = 0;
    fwrite(&root_a, sizeof(root_a), 1, fp);
    ac_size += sizeof(root_a);
    fwrite(&root_b, sizeof(root_b), 1, fp);
    ac_size += sizeof(root_b);
    fwrite(&root_c, sizeof(root_c), 1, fp);
    ac_size += sizeof(root_c);
    fwrite(&root_d, sizeof(root_d), 1, fp);
    ac_size += sizeof(root_d);
    char buffer[] = {'\0'};
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    //data - soubor cisla.txt
    fwrite(&cluster_a, sizeof(cluster_a), 1, fp);
    //data - soubor pohadka.txt
    fwrite(&cluster_b1, sizeof(cluster_b1), 1, fp);
    fwrite(&cluster_b2, sizeof(cluster_b2), 1, fp);
    fwrite(&cluster_b3, sizeof(cluster_b3), 1, fp);
    fwrite(&cluster_b4, sizeof(cluster_b4), 1, fp);
    fwrite(&cluster_b5, sizeof(cluster_b5), 1, fp);
    fwrite(&cluster_b6, sizeof(cluster_b6), 1, fp);
    fwrite(&cluster_b7, sizeof(cluster_b7), 1, fp);
    fwrite(&cluster_b8, sizeof(cluster_b8), 1, fp);
    fwrite(&cluster_b9, sizeof(cluster_b9), 1, fp);
    fwrite(&cluster_b10, sizeof(cluster_b10), 1, fp);
    fwrite(&cluster_b11, sizeof(cluster_b11), 1, fp);
    fwrite(&cluster_b12, sizeof(cluster_b12), 1, fp);
    fwrite(&cluster_b13, sizeof(cluster_b13), 1, fp);
    fwrite(&cluster_b14, sizeof(cluster_b14), 1, fp);
    fwrite(&cluster_b15, sizeof(cluster_b15), 1, fp);
    fwrite(&cluster_b16, sizeof(cluster_b16), 1, fp);
    fwrite(&cluster_b17, sizeof(cluster_b17), 1, fp);
    fwrite(&cluster_b18, sizeof(cluster_b18), 1, fp);
    fwrite(&cluster_b19, sizeof(cluster_b19), 1, fp);
    fwrite(&cluster_b20, sizeof(cluster_b20), 1, fp);
    fwrite(&cluster_b21, sizeof(cluster_b21), 1, fp);
    fwrite(&cluster_b22, sizeof(cluster_b22), 1, fp);
    fwrite(&cluster_b23, sizeof(cluster_b23), 1, fp);
    fwrite(&cluster_b24, sizeof(cluster_b24), 1, fp);
    //3x volne misto
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    //prazdny adresar sfad
    ac_size = 0;
    fwrite(&root_e, sizeof(root_e), 1, fp);
    ac_size += sizeof(root_e);
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    ac_size += sizeof(root_a);
    //prvni cast msg.txt
    fwrite(&cluster_c1, sizeof(cluster_c1), 1, fp);
    //sem je jedno co zapisu, sou to vadne sektory - tedy realne byto meli byt stringy FFFFFFcosi cosi cosiFFFFFF
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    //druha cast msg.txt
    fwrite(&cluster_c2, sizeof(cluster_c2), 1, fp);
    //prazdna slozka
    ac_size = 0;
    fwrite(&root_f, sizeof(root_f), 1, fp);
    ac_size += sizeof(root_f);
    fwrite(&root_g, sizeof(root_g), 1, fp);
    ac_size += sizeof(root_g);
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    ac_size += sizeof(root_a);

    fwrite(&cluster_f1, sizeof(cluster_f1), 1, fp);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    //zbytek disku budou 0
    for (long i = 37; i < br.usable_cluster_count; i++) {
        fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    }
    fclose(fp);
}