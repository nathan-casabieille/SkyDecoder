#!/usr/bin/env python3
import os
import sys
from pathlib import Path

def compile_cpp_files(directory, output_file):
    """
    Compile tous les fichiers .cpp et .h d'un répertoire en un seul fichier
    
    Args:
        directory (str): Chemin du répertoire à parcourir
        output_file (str): Nom du fichier de sortie
    """
    
    # Vérifier que le répertoire existe
    if not os.path.exists(directory):
        print(f"Erreur : Le répertoire '{directory}' n'existe pas.")
        return False
    
    # Extensions à rechercher
    extensions = ['.cpp', '.h']
    
    # Dossiers à ignorer
    ignored_dirs = ['build']
    
    # Collecter tous les fichiers
    files_found = []
    
    for root, dirs, files in os.walk(directory):
        # Supprimer les dossiers à ignorer de la liste des répertoires à parcourir
        dirs[:] = [d for d in dirs if d not in ignored_dirs]
        
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                file_path = os.path.join(root, file)
                files_found.append(file_path)
    
    # Trier les fichiers par nom pour un ordre cohérent
    files_found.sort()
    
    if not files_found:
        print(f"Aucun fichier .cpp ou .h trouvé dans '{directory}'")
        return False
    
    print(f"Trouvé {len(files_found)} fichier(s) :")
    for file in files_found:
        print(f"  - {file}")
    
    # Écrire le fichier de sortie
    try:
        with open(output_file, 'w', encoding='utf-8') as out_file:
            for file_path in files_found:
                # Obtenir le nom du fichier relatif au répertoire de base
                relative_path = os.path.relpath(file_path, directory)
                
                # Écrire l'en-tête du fichier
                out_file.write(f"{relative_path} :\n\n")
                
                # Lire et écrire le contenu du fichier
                try:
                    with open(file_path, 'r', encoding='utf-8') as in_file:
                        content = in_file.read()
                        out_file.write(content)
                        
                        # Ajouter une ligne vide si le fichier ne se termine pas par un retour à la ligne
                        if content and not content.endswith('\n'):
                            out_file.write('\n')
                            
                except UnicodeDecodeError:
                    # Essayer avec l'encodage latin-1 si utf-8 échoue
                    try:
                        with open(file_path, 'r', encoding='latin-1') as in_file:
                            content = in_file.read()
                            out_file.write(content)
                            if content and not content.endswith('\n'):
                                out_file.write('\n')
                    except Exception as e:
                        out_file.write(f"[Erreur lors de la lecture du fichier : {e}]\n")
                
                except Exception as e:
                    out_file.write(f"[Erreur lors de la lecture du fichier : {e}]\n")
                
                # Ajouter une ligne de séparation
                out_file.write("\n")
        
        print(f"Compilation terminée ! Fichier de sortie : {output_file}")
        return True
        
    except Exception as e:
        print(f"Erreur lors de l'écriture du fichier de sortie : {e}")
        return False

def main():
    """Fonction principale"""
    if len(sys.argv) < 2:
        print("Usage:")
        print(f"  {sys.argv[0]} <répertoire> [fichier_sortie]")
        print()
        print("Exemples:")
        print(f"  {sys.argv[0]} ./src")
        print(f"  {sys.argv[0]} ./src compiled_code.txt")
        print(f"  {sys.argv[0]} /path/to/project output.txt")
        return
    
    directory = sys.argv[1]
    
    # Nom du fichier de sortie par défaut
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        output_file = "compiled_files.txt"
    
    # Compiler les fichiers
    compile_cpp_files(directory, output_file)

if __name__ == "__main__":
    main()