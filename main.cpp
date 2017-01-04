#include <iostream>
#include <stdlib.h>
#include "fat.h"

using namespace std;

void resolve_arg(int argc, char *argv[]){
    fat fat1;
    if(argc < 3){
        std::cout << "Neplatný počet argumentů." << std::endl;
        exit(0);
    }
    else {
        char* fileName = argv[1];
        std::string action(argv[2]);
//        FILE *fsnew = fopen("nova.fat", "w+");
//        fat1.fat_creator(fsnew );
        fat1.fat_loader(fileName);
        if (action == "-a") {        // Nahraje soubor z adresáře do cesty virtuální FAT tabulky

        } else if (action == "-f") { // Smaže soubor s1 z vaseFAT.dat (s1 je plná cesta ve virtuální FAT)

        } else if (action == "-c") { // Vypíše čísla clusterů, oddělené dvojtečkou, obsahující data souboru s1 (s1 je plná cesta ve virtuální FAT)
            fat1.print_file_clusters(string(argv[3]));
        } else if (action == "-m") { // Vytvoří nový adresář ADR v cestě ADR2

        } else if (action == "-r") { // Smaže prázdný adresář ADR (ADR je plná cesta ve virtuální FAT)

        } else if (action == "-l") { // Vypíše obsah souboru s1 na obrazovku (s1 je plná cesta ve virtuální FAT)
            fat1.get_file_content(string(argv[3]));
        } else if (action == "-p") { // Vypíše obsah adresáře ve formátu +adresář, +podadresář cluster, ukončeno --, - soubor první_cluster počet_clusterů. Jeden záznam jeden řádek. Podadresáře odsazeny o /t:
            fat1.print_tree();
        } else if (action == "-g") { // defragmentace

        } else {
            std::cout << "Neplatný argument" << std::endl;
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    resolve_arg(argc, argv);
}

