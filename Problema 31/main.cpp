#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>

using namespace std;
using namespace pqxx;

// Funciones de utilidad
void limpiarPantalla() {
    system("clear");
}


void limpiarEntrada(){
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}


void esperarEntrada() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

// Función para ejecutar consultas SQL 
bool ejecutarSQL(connection &c, const string &query) {
    try {
        work W(c);
        W.exec(query);
        W.commit();
        return true;
    } catch (const exception &e) {
        cerr << "\033[1;31mError en la consulta SQL: " << e.what() << "\033[0m" << endl;
        return false;
    }
}


connection conectar() {
    const string CONFIGURACION_DB = 
        "dbname=discografia "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";

    try {
        connection c(CONFIGURACION_DB);
        if (!c.is_open()) {
            throw runtime_error("No se pudo establecer la conexión con la base de datos");
        }
        cout << "Conexión establecida con la base de datos: " << c.dbname() << endl;
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error al conectar: " + string(e.what()));
    }
    esperarEntrada();
}


void comprobarTablas(connection &c) {
    try {
        string query_companies = "CREATE TABLE IF NOT EXISTS Companies ("
                               "company_id SERIAL PRIMARY KEY, "
                               "name VARCHAR(100) NOT NULL, "
                               "address TEXT NOT NULL, "
                               "UNIQUE(name)"
                               ");";
        ejecutarSQL(c, query_companies);

        string query_recordings = "CREATE TABLE IF NOT EXISTS Recordings ("
                                "recording_id SERIAL PRIMARY KEY, "
                                "title VARCHAR(100) NOT NULL, "
                                "musical_category VARCHAR(50) NOT NULL, "
                                "track_count INTEGER NOT NULL CHECK (track_count > 0), "
                                "description TEXT, "
                                "company_id INTEGER REFERENCES Companies(company_id), "
                                "UNIQUE(title)"
                                ");";
        ejecutarSQL(c, query_recordings);

        string query_performers = "CREATE TABLE IF NOT EXISTS Performers ("
                                "performer_id SERIAL PRIMARY KEY, "
                                "name VARCHAR(100) NOT NULL, "
                                "biography TEXT, "
                                "UNIQUE(name)"
                                ");";
        ejecutarSQL(c, query_performers);

        string query_performances = "CREATE TABLE IF NOT EXISTS Performances ("
                                  "recording_id INTEGER REFERENCES Recordings(recording_id), "
                                  "performer_id INTEGER REFERENCES Performers(performer_id), "
                                  "participation_date DATE NOT NULL, "
                                  "PRIMARY KEY (recording_id, performer_id)"
                                  ");";
        ejecutarSQL(c, query_performances);

        string query_formats = "CREATE TABLE IF NOT EXISTS Formats ("
                             "format_id SERIAL PRIMARY KEY, "
                             "name VARCHAR(50) NOT NULL, "
                             "UNIQUE(name)"
                             ");";
        ejecutarSQL(c, query_formats);

        string query_physical_copies = "CREATE TABLE IF NOT EXISTS PhysicalCopies ("
                                     "copy_id SERIAL PRIMARY KEY, "
                                     "recording_id INTEGER REFERENCES Recordings(recording_id), "
                                     "format_id INTEGER REFERENCES Formats(format_id), "
                                     "conservation_state VARCHAR(20) CHECK "
                                     "(conservation_state IN ('bueno', 'malo', 'regular'))"
                                     ");";
        ejecutarSQL(c, query_physical_copies);

        cout << "Base de datos inicializada correctamente" << endl;
        system("sleep 1");
        cout << "Iniciando sistema de gestión..." << endl;
        system("sleep 2");
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
}

// Mostar datos
void mostrarCatalogo(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT r.recording_id, r.title, r.musical_category, r.track_count, "
            "COALESCE(r.description, 'Sin descripción') AS description, "
            "COALESCE(c.name, 'Sin compañía') AS company_name, "
            "COALESCE(string_agg(DISTINCT p.name, ', '), 'Sin intérpretes') AS performers, "
            "COALESCE(string_agg(DISTINCT f.name, ', '), 'Sin formatos') AS available_formats "
            "FROM Recordings r "
            "LEFT JOIN Companies c ON r.company_id = c.company_id "
            "LEFT JOIN Performances perf ON r.recording_id = perf.recording_id "
            "LEFT JOIN Performers p ON perf.performer_id = p.performer_id "
            "LEFT JOIN PhysicalCopies pc ON r.recording_id = pc.recording_id "
            "LEFT JOIN Formats f ON pc.format_id = f.format_id "
            "GROUP BY r.recording_id, r.title, r.musical_category, "
            "r.track_count, r.description, c.name "
            "ORDER BY r.title;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay grabaciones registradas en el catálogo." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== CATÁLOGO DE GRABACIONES ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Grabación #" << row["recording_id"].as<int>() << " ---" << endl;
            cout << "Título: " << row["title"].as<string>() << endl;
            cout << "Categoría: " << row["musical_category"].as<string>() << endl;
            cout << "Número de temas: " << row["track_count"].as<int>() << endl;
            cout << "Compañía: " << row["company_name"].as<string>() << endl;
            cout << "Intérpretes: " << row["performers"].as<string>() << endl;
            cout << "Formatos disponibles: " << row["available_formats"].as<string>() << endl;
            cout << "Descripción: " << row["description"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar el catálogo: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}


void mostrarInterpretes(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT p.performer_id, p.name, "
            "COALESCE(p.biography, 'Sin biografía') AS biography, "
            "COUNT(DISTINCT perf.recording_id) as recording_count, "
            "COALESCE(string_agg(DISTINCT r.title, ', '), 'Sin grabaciones') AS recordings "
            "FROM Performers p "
            "LEFT JOIN Performances perf ON p.performer_id = perf.performer_id "
            "LEFT JOIN Recordings r ON perf.recording_id = r.recording_id "
            "GROUP BY p.performer_id, p.name, p.biography "
            "ORDER BY p.performer_id;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay intérpretes registrados." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== CATÁLOGO DE INTÉRPRETES ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Intérprete #" << row["performer_id"].as<int>() << " ---" << endl;
            cout << "Nombre: " << row["name"].as<string>() << endl;
            cout << "Biografía: " << row["biography"].as<string>() << endl;
            cout << "Grabaciones (" << row["recording_count"].as<int>() << "): " 
                 << row["recordings"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar intérpretes: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}


void mostrarCopiasFisicas(connection &c) {
    limpiarPantalla();
    try {
        string query = 
            "SELECT pc.copy_id, r.title, f.name as format, "
            "pc.conservation_state, "
            "COALESCE(c.name, 'Sin compañía') as company "
            "FROM PhysicalCopies pc "
            "JOIN Recordings r ON pc.recording_id = r.recording_id "
            "JOIN Formats f ON pc.format_id = f.format_id "
            "LEFT JOIN Companies c ON r.company_id = c.company_id "
            "ORDER BY r.title, f.name;";

        nontransaction N(c);
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay copias físicas registradas." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== CATÁLOGO DE COPIAS FÍSICAS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Copia #" << row["copy_id"].as<int>() << " ---" << endl;
            cout << "Título: " << row["title"].as<string>() << endl;
            cout << "Formato: " << row["format"].as<string>() << endl;
            cout << "Estado: " << row["conservation_state"].as<string>() << endl;
            cout << "Compañía: " << row["company"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar copias físicas: " << e.what() << endl;
        esperarEntrada();
        throw;
    }
    esperarEntrada();
}

// Funciones para gestión de grabaciones
void registrarGrabacion(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        string titulo, categoria, descripcion;
        int num_temas, company_id;
        
        cout << "=== REGISTRO DE NUEVA GRABACIÓN ===" << endl;
        
        // Validación de título
        while (true) {
            cout << "Título (3-100 caracteres): ";
            getline(cin, titulo);
            if (titulo.length() >= 3 && titulo.length() <= 100) {
                break;
            }
            cout << "El título debe tener entre 3 y 100 caracteres." << endl;
        }
        
        // Validación de categoría
        while (true) {
            cout << "Categoría musical (3-50 caracteres): ";
            getline(cin, categoria);
            if (categoria.length() >= 3 && categoria.length() <= 50) {
                break;
            }
            cout << "La categoría debe tener entre 3 y 50 caracteres." << endl;
        }
        
        // Validación de número de temas
        while (true) {
            cout << "Número de temas (1-100): ";
            if (cin >> num_temas) {
                if (num_temas > 0 && num_temas <= 100) {
                    break;
                }
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Por favor, ingrese un número entre 1 y 100." << endl;
        }
        limpiarEntrada();
        
        // Descripción opcional
        cout << "Descripción (opcional, máximo 500 caracteres): ";
        getline(cin, descripcion);
        if (descripcion.length() > 500) {
            descripcion = descripcion.substr(0, 500);
            cout << "Descripción truncada a 500 caracteres." << endl;
        }

        // Selección de compañía
        result R_companies = W.exec("SELECT company_id, name FROM Companies ORDER BY company_id;");
        
        if (R_companies.empty()) {
            cout << "Error: No hay compañías registradas. Debe registrar una compañía primero." << endl;
            esperarEntrada();
            return;
        }

        cout << "\nCompañías disponibles:" << endl;
        for (result::const_iterator row = R_companies.begin(); row != R_companies.end(); ++row) {
            cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
        }
        
        // Validación de selección de compañía
        while (true) {
            cout << "Seleccione ID de compañía: ";
            if (cin >> company_id) {
                bool company_valida = false;
                for (result::const_iterator row = R_companies.begin(); row != R_companies.end(); ++row) {
                    if (row[0].as<int>() == company_id) {
                        company_valida = true;
                        break;
                    }
                }
                if (company_valida) break;
            }
            
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "ID de compañía no válido. Intente nuevamente." << endl;
        }
        limpiarEntrada();

        // Insertar grabación
        string query = "INSERT INTO Recordings (title, musical_category, track_count, description, company_id) "
                      "VALUES ($1, $2, $3, $4, $5) RETURNING recording_id;";
        result R = W.exec_params(query, titulo, categoria, num_temas, descripcion, company_id);
        int recording_id = R[0][0].as<int>();

        // Registro de intérpretes
        char mas_interpretes;
        do {
            cout << "\n=== Agregar Intérprete ===" << endl;
            
            result R_performers = W.exec("SELECT performer_id, name FROM Performers ORDER BY performer_id;");
            if (!R_performers.empty()) {
                cout << "\nIntérpretes existentes:" << endl;
                for (result::const_iterator row = R_performers.begin(); row != R_performers.end(); ++row) {
                    cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
                }
                cout << "0. Crear nuevo intérprete" << endl;
            }

            int performer_id;
            while (true) {
                cout << "Seleccione ID o 0 para nuevo: ";
                if (cin >> performer_id) {
                    bool id_valido = (performer_id == 0);
                    if (!id_valido && !R_performers.empty()) {
                        for (result::const_iterator row = R_performers.begin(); row != R_performers.end(); ++row) {
                            if (row[0].as<int>() == performer_id) {
                                id_valido = true;
                                break;
                            }
                        }
                    }
                    if (id_valido) break;
                }
                
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "ID no válido. Intente nuevamente." << endl;
            }
            limpiarEntrada();

            if (performer_id == 0) {
                string nombre, biografia;
                cout << "Nombre del intérprete: ";
                getline(cin, nombre);
                cout << "Biografía: ";
                getline(cin, biografia);

                query = "INSERT INTO Performers (name, biography) VALUES ($1, $2) RETURNING performer_id;";
                R = W.exec_params(query, nombre, biografia);
                performer_id = R[0][0].as<int>();
            }

            // Validación de fecha de participación
            string fecha;
            while (true) {
                cout << "Fecha de participación (YYYY-MM-DD): ";
                getline(cin, fecha);
                
                // Validación básica de formato de fecha
                bool fecha_valida = true;
                if (fecha.length() == 10 && 
                    fecha[4] == '-' && fecha[7] == '-') {
                    for (int i = 0; i < 10; ++i) {
                        if (i == 4 || i == 7) continue;
                        if (fecha[i] < '0' || fecha[i] > '9') {
                            fecha_valida = false;
                            break;
                        }
                    }
                } else {
                    fecha_valida = false;
                }
                
                if (fecha_valida) break;
                cout << "Formato de fecha inválido. Use YYYY-MM-DD." << endl;
            }

            query = "INSERT INTO Performances (recording_id, performer_id, participation_date) "
                   "VALUES ($1, $2, $3);";
            W.exec_params(query, recording_id, performer_id, fecha);

            cout << "¿Agregar otro intérprete? (s/n): ";
            cin >> mas_interpretes;
            limpiarEntrada();
        } while (mas_interpretes == 's' || mas_interpretes == 'S');

        // Registro de copias físicas
        char mas_copias;
        do {
            cout << "\n=== Agregar Copia Física ===" << endl;
            
            result R_formats = W.exec("SELECT format_id, name FROM Formats ORDER BY format_id;");
            if (R_formats.empty()) {
                cout << "No hay formatos disponibles. Debe registrar formatos primero." << endl;
                break;
            }

            cout << "\nFormatos disponibles:" << endl;
            for (result::const_iterator row = R_formats.begin(); row != R_formats.end(); ++row) {
                cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
            }

            int format_id;
            while (true) {
                cout << "Seleccione formato: ";
                if (cin >> format_id) {
                    bool formato_valido = false;
                    for (result::const_iterator row = R_formats.begin(); row != R_formats.end(); ++row) {
                        if (row[0].as<int>() == format_id) {
                            formato_valido = true;
                            break;
                        }
                    }
                    if (formato_valido) break;
                }
                
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Formato no válido. Intente nuevamente." << endl;
            }
            limpiarEntrada();

            string estado;
            while (true) {
                cout << "Estado de conservación (bueno/regular/malo): ";
                getline(cin, estado);
                if (estado == "bueno" || estado == "regular" || estado == "malo") {
                    break;
                }
                cout << "Estado no válido. Use bueno, regular o malo." << endl;
            }

            query = "INSERT INTO PhysicalCopies (recording_id, format_id, conservation_state) "
                   "VALUES ($1, $2, $3);";
            W.exec_params(query, recording_id, format_id, estado);

            cout << "¿Agregar otra copia física? (s/n): ";
            cin >> mas_copias;
            limpiarEntrada();
        } while (mas_copias == 's' || mas_copias == 'S');

        W.commit();
        cout << "\nGrabación registrada exitosamente." << endl;
        
    } catch (const exception &e) {
        cerr << "\nError durante el registro: " << e.what() << endl;
    }
    esperarEntrada();
}


void eliminarGrabacion(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        
        // Obtener grabaciones
        result R = W.exec("SELECT recording_id, title, musical_category FROM Recordings ORDER BY recording_id;");
        
        if (R.empty()) {
            cout << "No hay grabaciones registradas." << endl;
            esperarEntrada();
            return;
        }

        // Mostrar grabaciones disponibles
        cout << "\nGrabaciones disponibles:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << row[0].as<int>() << ". " << row[1].as<string>() 
                 << " (" << row[2].as<string>() << ")" << endl;
        }

        // Validación de ID de grabación
        int id;
        bool id_valido = false;
        do {
            cout << "\nIngrese ID de grabación a eliminar (0 para cancelar): ";
            if (cin >> id) {
                if (id == 0) {
                    cout << "Operación cancelada." << endl;
                    esperarEntrada();
                    return;
                }
                
                // Verificar si el ID existe
                for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                    if (row[0].as<int>() == id) {
                        id_valido = true;
                        break;
                    }
                }
                
                if (!id_valido) {
                    cout << "ID de grabación no válido. Intente nuevamente." << endl;
                }
            } else {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Entrada inválida. Intente nuevamente." << endl;
            }
        } while (!id_valido);
        limpiarEntrada();

        // Verificar existencia de la grabación
        result R_check = W.exec_params("SELECT title FROM Recordings WHERE recording_id = $1;", id);
        if (R_check.empty()) {
            cout << "Grabación no encontrada." << endl;
            esperarEntrada();
            return;
        }

        string titulo = R_check[0][0].as<string>();
        
        char confirm;
        do {
            cout << "¿Está seguro de eliminar \"" << titulo << "\"? (s/n): ";
            cin >> confirm;
            limpiarEntrada();
            
            if (confirm != 's' && confirm != 'S' && confirm != 'n' && confirm != 'N') {
                cout << "Respuesta no válida. Por favor, ingrese 's' o 'n'." << endl;
            }
        } while (confirm != 's' && confirm != 'S' && confirm != 'n' && confirm != 'N');

        if (confirm != 's' && confirm != 'S') {
            cout << "Operación cancelada." << endl;
            esperarEntrada();
            return;
        }

        // Conteo de registros relacionados
        result R_performances = W.exec_params("SELECT COUNT(*) FROM Performances WHERE recording_id = $1;", id);
        result R_physical_copies = W.exec_params("SELECT COUNT(*) FROM PhysicalCopies WHERE recording_id = $1;", id);
        
        int performances_count = R_performances[0][0].as<int>();
        int physical_copies_count = R_physical_copies[0][0].as<int>();

        cout << "\nRegistros a eliminar:" << endl;
        cout << "- Registros de interpretación: " << performances_count << endl;
        cout << "- Copias físicas: " << physical_copies_count << endl;

        // Eliminación en cascada
        W.exec_params("DELETE FROM Performances WHERE recording_id = $1;", id);
        W.exec_params("DELETE FROM PhysicalCopies WHERE recording_id = $1;", id);
        W.exec_params("DELETE FROM Recordings WHERE recording_id = $1;", id);

        W.commit();
        cout << "\nGrabación eliminada exitosamente." << endl;

    } catch (const exception &e) {
        cerr << "\nError crítico durante la eliminación: " << e.what() << endl;
    }
    esperarEntrada();
}


void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarPantalla();
        cout << R"(
 ╔════════════════════════════════════╗
 ║   SISTEMA DE GESTIÓN DISCOGRÁFICA  ║
 ╠════════════════════════════════════╣
 ║  1 │ Explorar Catálogo Completo    ║
 ║  2 │ Directorio de Intérpretes     ║
 ║  3 │ Inventario de Copias Físicas  ║
 ║  4 │ Registrar Nueva Grabación     ║
 ║  5 │ Gestionar Eliminaciones       ║
 ║  0 │ Cerrar Sistema                ║
 ╚════════════════════════════════════╝
)" << endl;
        cout << "\n  >> Seleccione una opción: ";

        // Validación de entrada
        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            opcion = -1;
        } else {
            limpiarEntrada();
        }

        try {
            switch (opcion) {
                case 1:
                    mostrarCatalogo(c); 
                    break;
                case 2: 
                    mostrarInterpretes(c); 
                    break;
                case 3: 
                    mostrarCopiasFisicas(c); 
                    break;
                case 4:
                    registrarGrabacion(c);
                    break;
                case 5:
                    eliminarGrabacion(c);
                    break;
                case 0:
                    limpiarPantalla();
                    cout << R"(
 ╔════════════════════════════════════╗
 ║     SISTEMA DE GESTIÓN MUSICAL     ║
 ╠════════════════════════════════════╣
 ║          Cerrando sistema...       ║
 ╚════════════════════════════════════╝
)" << endl;
                    system("sleep 1");
                    break;
                default:
                    limpiarPantalla();
                    cout << R"(
 ╔════════════════════════════════════╗
 ║           ERROR DE ENTRADA         ║
 ╠════════════════════════════════════╣
 ║   Opción no válida. Intente de     ║
 ║   nuevo seleccionando una opción   ║
 ║   entre 0 y 5.                     ║
 ╚════════════════════════════════════╝
)" << endl;
                    esperarEntrada();
                    break;
            }
        } catch (const exception &e) {
            cerr << "\n[ERROR] Operación fallida: " << e.what() << endl;
            esperarEntrada();
        }
    } while (opcion != 0);
}


int main() {
    limpiarPantalla();
    
    cout << R"(
 ╔════════════════════════════════════╗
 ║     SISTEMA DE GESTIÓN MUSICAL     ║
 ╠════════════════════════════════════╣
 ║    Iniciando sistema de gestión    ║
 ║          de discografía            ║
 ╚════════════════════════════════════╝
)" << endl;
    
    try {
        cout << "\n  Conectando con la base de datos..." << endl;
        connection c = conectar();
        system("sleep 1");
        
        cout << "\n  Verificando estructura de datos..." << endl;
        comprobarTablas(c);
        system("sleep 1");
        
        cout << R"(
 ╔════════════════════════════════════╗
 ║     SISTEMA DE GESTIÓN MUSICAL     ║
 ╠════════════════════════════════════╣
 ║      Sistema inicializado con      ║
 ║           éxito total              ║
 ╚════════════════════════════════════╝
)" << endl;
        system("sleep 1");
        
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << R"(
 ╔════════════════════════════════════╗
 ║           ERROR CRÍTICO            ║
 ╠════════════════════════════════════╣
 ║   El sistema no pudo inicializarse ║
 ║   Detalles del error:              ║
 ║   )" << e.what() << R"(
 ╚════════════════════════════════════╝
)" << endl;
        esperarEntrada();
        return 1;
    }
    
    cout << R"(
 ╔════════════════════════════════════╗
 ║     SISTEMA DE GESTIÓN MUSICAL     ║
 ╠════════════════════════════════════╣
 ║    Gracias por usar el sistema     ║
 ║         de discografía             ║
 ╚════════════════════════════════════╝
)" << endl;
    system("sleep 1");
    return 0;
}