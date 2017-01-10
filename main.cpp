#include <iostream>
#include <stdlib.h>
#include "fat.h"

using namespace std;

/**
 * Function checking the correct count of arguments for called function, if a wrong number of parameters is passed, the program ends
 * @param action Called function
 * @param argc Number of passed paramenters
 * @param req Number of parameters required for called function
 */
void compare_args(string action, int argc, int req) {
    if (argc != req) {
        std::cout << "WRONG NUMBER OF ARGUMENTS FOR ACTION '" << action << "', EXPECTED " << req - 1 << std::endl;
        exit(1);
    }
}

/**
 * Function that parses input arguments and calls the appropriate function
 * @param argc Argument count
 * @param argv Argument vector
 */
void resolve_arg(int argc, char *argv[]) {
    fat fat1;
    if (argc < 3) {
        std::cout << "NOT ENOUGH ARGUMENTS." << std::endl;
        exit(0);
    } else {
        char *fileName = argv[1];
        std::string action(argv[2]);
        fat1.fat_loader(fileName);
        if (action == "-a") {        // Nahraje soubor z adresáře do cesty virtuální FAT tabulky
            compare_args(action, argc, 5);
            fat1.create_new_file(string(argv[3]), string(argv[4]));
        } else if (action == "-f") { // Smaže soubor s1 z vaseFAT.dat (s1 je plná cesta ve virtuální FAT)
            compare_args(action, argc, 4);
            fat1.delete_file(string(argv[3]));
        } else if (action == "-c") { // Vypíše čísla clusterů, oddělené dvojtečkou, obsahující data souboru s1 (s1 je plná cesta ve virtuální FAT)
            compare_args(action, argc, 4);
            fat1.print_file_clusters(string(argv[3]));
        } else if (action == "-m") { // Vytvoří nový adresář ADR v cestě ADR2
            compare_args(action, argc, 5);
            fat1.create_new_directory(string(argv[3]), string(argv[4]));
        } else if (action == "-r") { // Smaže prázdný adresář ADR (ADR je plná cesta ve virtuální FAT)
            compare_args(action, argc, 4);
            fat1.delete_directory(string(argv[3]));
        } else if (action == "-l") { // Vypíše obsah souboru s1 na obrazovku (s1 je plná cesta ve virtuální FAT)
            compare_args(action, argc, 4);
            fat1.get_file_content(string(argv[3]));
        } else if (action == "-p") { // Vypíše obsah adresáře ve formátu +adresář, +podadresář cluster, ukončeno --, - soubor první_cluster počet_clusterů. Jeden záznam jeden řádek. Podadresáře odsazeny o /t:
            compare_args(action, argc, 3);
            fat1.print_tree();
        } else if (action == "-g") { // defragmentace
            compare_args(action, argc, 3);
            fat1.defragment();
        } else {
            std::cout << "ARGUMENT NOT RECOGNIZED" << std::endl;
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    resolve_arg(argc, argv);
}

