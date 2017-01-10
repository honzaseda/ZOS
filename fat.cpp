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
        if ((fs = fopen(name, "w+")) != NULL) {
            fat_creator(fs);
            fs = fopen(name, "r+");
        } else {
            exit(1);
        }

    }
    fseek(fs, SEEK_SET, 0);
    fread(fs_br, sizeof(struct boot_record), 1, fs);

    fat_table = (int32_t *) malloc(sizeof(int32_t) * fs_br->usable_cluster_count);
    for (int l = 0; l < fs_br->fat_copies; l++) {
        fread(fat_table, sizeof(*fat_table) * fs_br->usable_cluster_count, 1, fs);
    }

    //print_info();
    set_cluster_data();
    return 0;
}

/**
 * Stores FAT data (file information, cluster content) into appropriate vectors
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
    if((fnew = fopen(file_path.c_str(), "r")) == NULL){
        std::cout << "FILE NOT FOUND" << std::endl;
        exit(1);
    }
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
    std::string file_name = file_path.substr(file_path.find_last_of("/") + 1);
    std::string name = file_name.substr(0, file_name.find_last_of(".")).substr(0,8);
    std::string extension = file_name.substr(file_name.find_last_of(".") + 1).substr(0,3);
    std::string cropped_file_name = name + "." + extension;

    memset(root.name, '\0', sizeof(root.name));
    root.is_file = 1;
    strcpy(root.name, cropped_file_name.c_str());
    root.size = fsize;
    root.start_cluster = free_clusters.at(0);

    int32_t parent = get_parent_cluster(result);
    if (parent < 0) {
        std::cout << "PATH NOT FOUND" << std::endl;
        exit(1);
    }
    std::vector<fat::directory> children = get_dir_children(parent);
    long max_cluster_dirs = fs_br->cluster_size / sizeof(struct directory);
    if(children.size() >= max_cluster_dirs){
        std::cout << "DIRECTORY IS FULL" << std::endl;
        exit(1);
    }
    for(int i = 0; i < children.size(); i++){
        if((strcmp(children.at(i).name, cropped_file_name.c_str()) == 0) && children.at(i).is_file){
            std::cout << "NAME ALREADY EXISTS" << std::endl;
            exit(1);
        }
    }
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
                            break;
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
    strcpy(root.name, dir_name.substr(0,12).c_str());
    root.size = 0;
    root.start_cluster = cluster;

    int32_t parent = get_parent_cluster(result);
    if (parent < 0) {
        std::cout << "PATH NOT FOUND" << std::endl;
        exit(1);
    }
    long max_cluster_dirs = fs_br->cluster_size / sizeof(struct directory);
    std::vector<fat::directory> children = get_dir_children(parent);
    if(children.size() >= max_cluster_dirs){
        std::cout << "DIRECTORY IS FULL" << std::endl;
        exit(1);
    }
    for(int i = 0; i < children.size(); i++){
        if((strcmp(children.at(i).name, dir_name.substr(0,12).c_str()) == 0) && !children.at(i).is_file){
            std::cout << "NAME ALREADY EXISTS" << std::endl;
            exit(1);
        }
    }
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
    if(result.size() == 0){
        std::cout << "CANNOT DELETE ROOT" << std::endl;
        exit(1);
    }
    int32_t directory = get_parent_cluster(result);
    std::vector<fat::directory> dir_children = get_dir_children(directory);
    if (dir_children.size() > 0) {
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
        if (parent_children.at(j).start_cluster != directory) {
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

    std::cout << "OK" << std::endl;
}

/**
 * Deletes file and its record in FAT table and parent directory, file is specified by file path
 * @param file_path path to the file
 */
void fat::delete_file(std::string file_path) {
    std::vector<std::string> result = explode(file_path, '/');

    int32_t file_start = get_file_start(result);

    if (file_start <= 0) {
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
        if (parent_children.at(j).start_cluster != file_start) {
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

    int32_t curr_cluster = file_start;
    std::vector<int32_t> clusters = std::vector<int32_t>();
    while (new_fat[curr_cluster] != FAT_FILE_END) {
        int32_t tmp = new_fat[curr_cluster];
        new_fat[curr_cluster] = FAT_UNUSED;
        curr_cluster = tmp;
        clusters.push_back(tmp);
    }
    clusters.push_back(curr_cluster);
    new_fat[curr_cluster] = FAT_UNUSED;

    fseek(fs, sizeof(boot_record), SEEK_SET);
    for (int i = 0; i < fs_br->fat_copies; i++) {
        fwrite(&new_fat, sizeof(new_fat), 1, fs);
    }

    // ---
    // Erasing directory 'content' in FAT, just to be sure
    // ---
    char cluster_empty[fs_br->cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    for (int i = 0; i < clusters.size(); i++) {
        fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
                   clusters.at(i) * fs_br->cluster_size), SEEK_SET);
        fwrite(&cluster_empty, sizeof(cluster_empty), 1, fs);
    }

    std::cout << "OK" << std::endl;
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
        std::cout << std::endl;
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
 * Function for defragmenting the loaded FAT file system
 */
void fat::defragment() {
    std::vector<new_records> new_rec = std::vector<new_records>();
    int32_t new_fat[fs_br->usable_cluster_count];
    new_records temp;

    std::cout << "DEFRAGMENTING FILE SYSTEM\n-------------------------" << std::endl;

    int32_t fat_pos = 1;
    for (int i = 0; i < dir_info.size(); i++) {
        std::vector<int32_t> clusters = get_file_clusters(dir_info.at(i).dir->start_cluster);
        if (dir_info.at(i).dir->is_file) {
            std::cout << dir_info.at(i).dir->name << std::endl;
            std::cout << "\tclusters before: " << clusters.at(0);
            for (int k = 1; k < clusters.size(); k++) {
                std::cout << ", " << clusters.at(k);
            }
            std::cout << std::endl << "\tclusters after:  ";
            for (int j = 0; j < clusters.size() - 1; j++) {
                temp.actual_cluster = fat_pos;
                temp.actual_child = fat_pos + 1;
                temp.orig_cluster = clusters.at(j);
                new_rec.push_back(temp);
                std::cout << fat_pos << ", ";
                fat_pos++;
            }
            std::cout << fat_pos << std::endl;
            temp.actual_cluster = fat_pos;
            temp.actual_child = FAT_FILE_END;
            temp.orig_cluster = clusters.at(clusters.size() - 1);
            new_rec.push_back(temp);
            fat_pos++;
        } else {
            std::cout << dir_info.at(i).dir->name << std::endl;
            std::cout << "\tclusters before: " << clusters.at(0) << std::endl;
            temp.actual_cluster = fat_pos;
            temp.actual_child = FAT_DIRECTORY;
            temp.orig_cluster = clusters.at(0);
            new_rec.push_back(temp);
            std::cout << "\tclusters after:  " << fat_pos << std::endl;
            fat_pos++;
        }
    }
    new_fat[0] = FAT_DIRECTORY;
    for (int i = 0; i < new_rec.size(); i++) {
        new_fat[new_rec.at(i).actual_cluster] = new_rec.at(i).actual_child;
    }
    for (int i = new_rec.size() + 1; i < fs_br->usable_cluster_count; i++) {
        new_fat[i] = FAT_UNUSED;
    }
    fseek(fs, sizeof(boot_record), SEEK_SET);
    for (int i = 0; i < fs_br->fat_copies; i++) {
        fwrite(&new_fat, sizeof(new_fat), 1, fs);
    }

    for (int i = 0; i < fs_br->usable_cluster_count; i++) {
        int32_t orig = get_orig_cluster(new_rec, i);
        if (new_fat[i] == FAT_DIRECTORY) {
            std::vector<fat::directory> dir_children = get_dir_children(orig);
            for (int j = 0; j < dir_children.size(); j++) {
                dir_children.at(j).start_cluster = get_new_cluster(new_rec, dir_children.at(j).start_cluster);
            }
            int16_t ac_size = 0;
            fseek(fs, (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
                       i * fs_br->cluster_size), SEEK_SET);
            for (int j = 0; j < dir_children.size(); j++) {
                fwrite(&dir_children.at(j), sizeof(fat::directory), 1, fs);
                ac_size += sizeof(fat::directory);
            }
            char buffer[] = {'\0'};
            for (int16_t j = 0; j < (fs_br->cluster_size - ac_size); j++) {
                fwrite(buffer, sizeof(buffer), 1, fs);
            }
        } else {
            if ((i != orig) && (new_fat[i] != FAT_UNUSED)) {
                char cluster[fs_br->cluster_size];
                memset(cluster, '\0', sizeof(cluster));
                strcpy(cluster, cluster_data.at(orig).c_str());
                fseek(fs,
                      (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
                       (fs_br->cluster_size * i)),
                      SEEK_SET);
                fwrite(&cluster, sizeof(cluster), 1, fs);
            } else if (new_fat[i] == FAT_UNUSED) {
                char cluster_empty[fs_br->cluster_size];
                memset(cluster_empty, '\0', sizeof(cluster_empty));
                fseek(fs,
                      (sizeof(boot_record) + (fs_br->fat_copies * sizeof(*fat_table) * fs_br->usable_cluster_count) +
                       (fs_br->cluster_size * i)),
                      SEEK_SET);
                fwrite(&cluster_empty, sizeof(cluster_empty), 1, fs);
            }
        }
    }

    std::cout << "------------------------\nFILE SYSTEM DEFRAGMENTED" << std::endl;
}

/**
 * Function for getting the position of a cluster before defragmentation
 * @param rec Vector of structures containing defragmented clusters
 * @param actual_cluster Number of cluster after defragmentation
 * @return Integer value of cluster number before the defragmentation
 */
int32_t fat::get_orig_cluster(std::vector<new_records> rec, int32_t actual_cluster) {
    for (int i = 0; i < rec.size(); i++) {
        if (rec.at(i).actual_cluster == actual_cluster) {
            return rec.at(i).orig_cluster;
        }
    }
}

/**
 * Function for getting the position of a cluster after defragmentation
 * @param rec Vector of structures containing defragmented clusters
 * @param new_cluster Number of a cluster before defragmentation
 * @return Integer value of cluster number after the defragmentation
 */
int32_t fat::get_new_cluster(std::vector<new_records> rec, int32_t new_cluster) {
    for (int i = 0; i < rec.size(); i++) {
        if (rec.at(i).orig_cluster == new_cluster) {
            return rec.at(i).actual_cluster;
        }
    }
}

/**
 * Function for getting all clusters that the file content is stored on
 * @param cluster Starting cluster number
 * @return A vector of clusters
 */
std::vector<int32_t> fat::get_file_clusters(int32_t cluster) {
    std::vector<int32_t> clusters = std::vector<int32_t>();
    if ((fat_table[cluster] == FAT_DIRECTORY) || (fat_table[cluster] == FAT_FILE_END)) {
        clusters.push_back(cluster);
    } else {
        int32_t curr_cluster = cluster;
        clusters.push_back(curr_cluster);
        while (fat_table[curr_cluster] != FAT_FILE_END) {
            clusters.push_back(fat_table[curr_cluster]);
            curr_cluster = fat_table[curr_cluster];
        }
    }
    return clusters;
}

/**
 * Prints out FAT Boot record and FAT table content (does not print empty clusters).
 * Only for debugging purposes
 */
void fat::print_info() {
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "BOOT RECORD" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "volume_descriptor: " << fs_br->volume_descriptor << std::endl;
    printf("fat_type: %d\n", fs_br->fat_type);
    printf("fat_copies: %d\n", fs_br->fat_copies);
    std::cout << "cluster_size: " << fs_br->cluster_size << std::endl;
    std::cout << "usable_cluster_count: " << fs_br->usable_cluster_count << std::endl;
    std::cout << "signature: " << fs_br->signature << std::endl;

    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "FAT table" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    for (int i = 0; i < fs_br->usable_cluster_count; i++) {
        //if (fat_table[i] != FAT_UNUSED) {
        printf("[%d] %d\n", i, fat_table[i]);
        //}
    }
}

/**
 * Creating an empty new FAT, default FAT options can be edited in the header file
 * @param fp Pointer to a file stream, where the FAT will be stored
 */
void fat::fat_creator(FILE *fp) {
    struct boot_record br;
    memset(br.volume_descriptor, '\0', sizeof(br.volume_descriptor));
    memset(br.signature, '\0', sizeof(br.signature));
    char volume_descriptor[250] = "FAT description";
    strcpy(br.volume_descriptor, volume_descriptor);
    strcpy(br.signature, "A14B0448P");
    br.fat_type = NEW_FAT_TYPE;
    br.fat_copies = NEW_FAT_COPIES;
    br.cluster_size = NEW_CLUSTER_SIZE;
    br.usable_cluster_count = pow(2.0, NEW_FAT_TYPE) - NEW_FAT_COPIES - 2;

    //empty cluster
    char cluster_empty[br.cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));

    // ---
    // Initializing FAT table with ROOT directory
    // ---
    int32_t fat[br.usable_cluster_count];
    fat[0] = FAT_DIRECTORY;
    for (int32_t i = 1; i < br.usable_cluster_count; i++) {
        fat[i] = FAT_UNUSED;
    }

    //boot record
    fwrite(&br, sizeof(br), 1, fp);
    // 2x FAT copies
    fwrite(&fat, sizeof(fat), 1, fp);
    fwrite(&fat, sizeof(fat), 1, fp);
    //dir - bacha, tady je potreba zapsat CELY CLUSTER, ne jen prvnich n-BYTU plozek - tedy doplnit nulami poradi zaznamu v directory NEMUSI odpovidat poradi ve FATce a na disku
    //zbytek disku budou 0
    for (long i = 0; i < br.usable_cluster_count; i++) {
        fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    }
    fclose(fp);
}