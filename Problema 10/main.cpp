#include <iostream>
#include <pqxx/pqxx>
#include <string>
#include <iomanip>
#include <limits>

using namespace std;
using namespace pqxx;

void limpiarConsola() {
    system("clear");
}

void limpiarBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausar() {
    cout << "\nPresione cualquier tecla para continuar...";
    cin.get();
}

bool validarNIF(const string& nif) {
    return nif.length() == 9 && all_of(nif.begin(), nif.end(), ::isalnum);
}

string validarTexto(const string& prompt, size_t minLen, size_t maxLen) {
    string text;
    bool valid = false;
    
    do {
        cout << prompt;
        getline(cin, text);
        
        if (text.length() < minLen || text.length() > maxLen) {
            cout << "Error: El texto debe tener entre " << minLen << " y " << maxLen << " caracteres\n";
        } else {
            valid = true;
        }
    } while (!valid);
    
    return text;
}

double validarDouble(const string& prompt, double min, double max) {
    double value;
    bool valid = false;
    string input;
    
    do {
        cout << prompt;
        getline(cin, input);
        
        try {
            value = stod(input);
            if (value >= min && value <= max) {
                valid = true;
            } else {
                cout << "Error: El valor debe estar entre " << min << " y " << max << "\n";
            }
        } catch (...) {
            cout << "Error: Ingrese un número válido\n";
        }
    } while (!valid);
    
    return value;
}

int validarInt(const string& prompt, int min, int max) {
    int value;
    bool valid = false;
    string input;
    
    do {
        cout << prompt;
        getline(cin, input);
        
        try {
            value = stoi(input);
            if (value >= min && value <= max) {
                valid = true;
            } else {
                cout << "Error: El valor debe estar entre " << min << " y " << max << "\n";
            }
        } catch (...) {
            cout << "Error: Ingrese un número entero válido\n";
        }
    } while (!valid);
    
    return value;
}

connection conectar() {
    const string DB_CONFIG = 
        "dbname=publicidad "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";

    try {
        connection c(DB_CONFIG);
        if (!c.is_open()) {
            throw runtime_error("No se pudo conectar a la base de datos");
        }
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error de conexión: " + string(e.what()));
    }
}

void comprobarTablas(connection &c) {
    try {
        work W(c);
        
        // Empresas de medios
        W.exec("CREATE TABLE IF NOT EXISTS EmpresasMedios ("
               "nif VARCHAR(9) PRIMARY KEY,"
               "nombre VARCHAR(100) NOT NULL,"
               "director VARCHAR(100) NOT NULL,"
               "direccion TEXT NOT NULL"
               ");");

        // Emisoras
        W.exec("CREATE TABLE IF NOT EXISTS Emisoras ("
               "nif VARCHAR(9) PRIMARY KEY,"
               "nombre VARCHAR(100) NOT NULL,"
               "direccion TEXT NOT NULL,"
               "director VARCHAR(100) NOT NULL,"
               "banda_hz NUMERIC(10,2) UNIQUE NOT NULL,"
               "provincia VARCHAR(50) NOT NULL"
               ");");

        // Cadenas Radio
        W.exec("CREATE TABLE IF NOT EXISTS CadenasRadio ("
               "cadena_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) UNIQUE NOT NULL,"
               "director VARCHAR(100) NOT NULL,"
               "emisora_sede_nif VARCHAR(9) REFERENCES Emisoras(nif),"
               "empresa_nif VARCHAR(9) REFERENCES EmpresasMedios(nif)"
               ");");

        // Emisoras_Cadena
        W.exec("CREATE TABLE IF NOT EXISTS EmisorasCadena ("
               "cadena_id INTEGER REFERENCES CadenasRadio(cadena_id),"
               "emisora_nif VARCHAR(9) REFERENCES Emisoras(nif),"
               "PRIMARY KEY (cadena_id, emisora_nif)"
               ");");

        // Programas
        W.exec("CREATE TABLE IF NOT EXISTS Programas ("
               "programa_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) UNIQUE NOT NULL,"
               "responsable VARCHAR(100) NOT NULL,"
               "precio_segundo NUMERIC(10,2) NOT NULL,"
               "cadena_id INTEGER REFERENCES CadenasRadio(cadena_id),"
               "emisora_local_nif VARCHAR(9) REFERENCES Emisoras(nif),"
               "CHECK (cadena_id IS NOT NULL OR emisora_local_nif IS NOT NULL)"
               ");");

        // Franjas Horarias
        W.exec("CREATE TABLE IF NOT EXISTS FranjasHorarias ("
               "franja_id SERIAL PRIMARY KEY,"
               "programa_id INTEGER REFERENCES Programas(programa_id),"
               "dia_semana INTEGER CHECK (dia_semana BETWEEN 1 AND 7),"
               "hora_inicio TIME NOT NULL,"
               "duracion_minutos INTEGER NOT NULL"
               ");");

        // Patrocinadores
        W.exec("CREATE TABLE IF NOT EXISTS Patrocinadores ("
               "contrato_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) NOT NULL,"
               "duracion_contrato INTEGER NOT NULL,"
               "importe_total NUMERIC(10,2) NOT NULL"
               ");");

        // Publicidad
        W.exec("CREATE TABLE IF NOT EXISTS Publicidad ("
               "programa_id INTEGER REFERENCES Programas(programa_id),"
               "contrato_id INTEGER REFERENCES Patrocinadores(contrato_id),"
               "segundos_semana INTEGER NOT NULL,"
               "PRIMARY KEY (programa_id, contrato_id)"
               ");");

        W.commit();
    } catch (const exception &e) {
        throw runtime_error("Error inicializando tablas: " + string(e.what()));
    }
}

void registrarEmisora(connection &c) {
    limpiarConsola();
    cout << "\n=== REGISTRO DE EMISORA DE RADIO ===\n";
    
    try {
        // Verificar banda hertziana única por provincia
        string provincia = validarTexto("Provincia: ", 3, 50);
        double banda_hz;
        bool banda_valida = false;
        
        do {
            banda_hz = validarDouble("Banda Hertziana (MHz): ", 0.1, 999.9);
            
            nontransaction N(c);
            result R = N.exec_params(
                "SELECT 1 FROM Emisoras WHERE provincia = $1 AND banda_hz = $2",
                provincia, banda_hz
            );
            
            if (!R.empty()) {
                cout << "Error: Ya existe una emisora con esta banda en la provincia\n";
            } else {
                banda_valida = true;
            }
        } while (!banda_valida);

        string nif;
        bool nif_valid = false;
        do {
            cout << "NIF (9 caracteres): ";
            getline(cin, nif);
            if (!validarNIF(nif)) {
                cout << "Error: NIF inválido. Debe tener 9 caracteres alfanuméricos\n";
            } else {
                nontransaction N(c);
                result R = N.exec_params("SELECT 1 FROM Emisoras WHERE nif = $1", nif);
                if (!R.empty()) {
                    cout << "Error: Ya existe una emisora con este NIF\n";
                } else {
                    nif_valid = true;
                }
            }
        } while (!nif_valid);
        
        string nombre = validarTexto("Nombre: ", 3, 100);
        string direccion = validarTexto("Dirección: ", 5, 200);
        string director = validarTexto("Director: ", 3, 100);

        work W(c);
        W.exec_params(
            "INSERT INTO Emisoras (nif, nombre, direccion, director, banda_hz, provincia) "
            "VALUES ($1, $2, $3, $4, $5, $6)",
            nif, nombre, direccion, director, banda_hz, provincia
        );
        W.commit();
        
        cout << "\n✓ Emisora registrada exitosamente\n";
        
    } catch (const exception &e) {
        cout << "\n✘ Error: " << e.what() << endl;
    }
    pausar();
}

void registrarPublicidad(connection &c) {
    limpiarConsola();
    cout << "\n=== REGISTRO DE PUBLICIDAD ===\n";
    
    try {
        nontransaction N(c);
        
        // Verificar patrocinadores
        result patrocinadores = N.exec(
            "SELECT contrato_id, nombre, duracion_contrato, importe_total "
            "FROM Patrocinadores ORDER BY contrato_id"
        );
        
        if (patrocinadores.empty()) {
            cout << "\n✘ Error: No hay patrocinadores registrados. Registre un patrocinador primero.\n";
            pausar();
            return;
        }

        // Verificar programas
        result programas = N.exec(
            "SELECT p.programa_id, p.nombre, p.precio_segundo, "
            "COALESCE(c.nombre, e.nombre) as emisor "
            "FROM Programas p "
            "LEFT JOIN CadenasRadio c ON p.cadena_id = c.cadena_id "
            "LEFT JOIN Emisoras e ON p.emisora_local_nif = e.nif "
            "ORDER BY p.programa_id"
        );
        
        if (programas.empty()) {
            cout << "\n✘ Error: No hay programas registrados. Registre un programa primero.\n";
            pausar();
            return;
        }

        cout << "\nProgramas disponibles:\n";
        cout << "----------------------------------------\n";
        for (const auto &row : programas) {
            cout << "ID: " << row["programa_id"].as<int>() 
                 << " | " << row["nombre"].as<string>()
                 << " | Emisor: " << row["emisor"].as<string>() 
                 << " | Precio/seg: $" << row["precio_segundo"].as<double>() << "\n";
        }
        
        int programa_id = validarInt("Seleccione programa ID: ", 1, 999999);
        
        result verify_prog = N.exec_params(
            "SELECT nombre, precio_segundo FROM Programas WHERE programa_id = $1",
            programa_id
        );
        if (verify_prog.empty()) {
            cout << "\n✘ Programa no encontrado\n";
            pausar();
            return;
        }

        cout << "\nPatrocinadores disponibles:\n";
        cout << "----------------------------------------\n";
        for (const auto &row : patrocinadores) {
            cout << "ID: " << row["contrato_id"].as<int>() 
                 << " | " << row["nombre"].as<string>()
                 << " | Duración: " << row["duracion_contrato"].as<int>() << " días"
                 << " | Importe: $" << row["importe_total"].as<double>() << "\n";
        }
        
        int contrato_id = validarInt("Seleccione contrato ID: ", 1, 999999);
        
        result verify_pat = N.exec_params(
            "SELECT nombre FROM Patrocinadores WHERE contrato_id = $1",
            contrato_id
        );
        if (verify_pat.empty()) {
            cout << "\n✘ Patrocinador no encontrado\n";
            pausar();
            return;
        }

        result verify_pub = N.exec_params(
            "SELECT 1 FROM Publicidad WHERE programa_id = $1 AND contrato_id = $2",
            programa_id, contrato_id
        );
        if (!verify_pub.empty()) {
            cout << "\n✘ Ya existe publicidad para este programa y patrocinador\n";
            pausar();
            return;
        }

        int segundos = validarInt("Segundos por semana (1-3600): ", 1, 3600);

        // Calcular y mostrar costo antes de confirmar
        double precio_segundo = verify_prog[0]["precio_segundo"].as<double>();
        double costo_semanal = segundos * precio_segundo;
        
        cout << "\nResumen de la publicidad:\n";
        cout << "----------------------------------------\n";
        cout << "Programa: " << verify_prog[0]["nombre"].as<string>() << "\n";
        cout << "Patrocinador: " << verify_pat[0]["nombre"].as<string>() << "\n";
        cout << "Segundos por semana: " << segundos << "\n";
        cout << "Costo semanal: $" << fixed << setprecision(2) << costo_semanal << "\n";
        
        cout << "\n¿Confirmar registro? (S/N): ";
        char confirma;
        cin >> confirma;
        limpiarBuffer();

        N.abort(); 

        if (toupper(confirma) == 'S') {
            try {
                work W(c);
                W.exec_params(
                    "INSERT INTO Publicidad (programa_id, contrato_id, segundos_semana) "
                    "VALUES ($1, $2, $3)",
                    programa_id, contrato_id, segundos
                );
                W.commit();
                cout << "\n✓ Publicidad registrada exitosamente\n";
            } catch (const exception &e) {
                cout << "\n✘ Error al registrar: " << e.what() << endl;
            }
        } else {
            cout << "\nOperación cancelada\n";
        }
    } catch (const exception &e) {
        cout << "\n✘ Error: " << e.what() << endl;
    }
    pausar();
}

void eliminarPublicidad(connection &c) {
   limpiarConsola();
   cout << "\n=== ELIMINAR PUBLICIDAD ===\n";
   
   try {
       string current_program = "";
       {
           nontransaction N(c);
           result R = N.exec(
               "WITH PublicidadCompleta AS ("
               "    SELECT "
               "        p.programa_id,"
               "        p.nombre as programa,"
               "        pr.contrato_id,"
               "        pr.nombre as patrocinador,"
               "        pub.segundos_semana,"
               "        (pub.segundos_semana * p.precio_segundo) as costo_semanal,"
               "        COALESCE(c.nombre, e.nombre) as emisor,"
               "        p.precio_segundo "
               "    FROM Publicidad pub "
               "    JOIN Programas p ON pub.programa_id = p.programa_id "
               "    JOIN Patrocinadores pr ON pub.contrato_id = pr.contrato_id "
               "    LEFT JOIN CadenasRadio c ON p.cadena_id = c.cadena_id "
               "    LEFT JOIN Emisoras e ON p.emisora_local_nif = e.nif "
               "    ORDER BY p.nombre, pr.nombre"
               ") "
               "SELECT *, "
               "    SUM(costo_semanal) OVER (PARTITION BY programa) as total_programa "
               "FROM PublicidadCompleta"
           );

           if (R.empty()) {
               cout << "\nNo hay publicidad registrada en el sistema\n";
               pausar();
               return;
           }

           for (const auto &row : R) {
               if (row["programa"].as<string>() != current_program) {
                   if (!current_program.empty()) {
                       cout << "Total programa: $" << fixed << setprecision(2) 
                            << row["total_programa"].as<double>() << "\n";
                       cout << "----------------------------------------\n";
                   }
                   current_program = row["programa"].as<string>();
                   cout << "\nPrograma: " << current_program << "\n";
                   cout << "Emisor: " << row["emisor"].as<string>() << "\n";
                   cout << "----------------------------------------\n";
               }
               
               cout << "ID Programa: " << row["programa_id"].as<int>() << "\n";
               cout << "ID Contrato: " << row["contrato_id"].as<int>() << "\n";
               cout << "Patrocinador: " << row["patrocinador"].as<string>() << "\n";
               cout << "Segundos/semana: " << row["segundos_semana"].as<int>() << "\n";
               cout << "Costo semanal: $" << fixed << setprecision(2) 
                    << row["costo_semanal"].as<double>() << "\n\n";
           }

           if (!current_program.empty()) {
               cout << "Total programa: $" << fixed << setprecision(2) 
                    << R[R.size()-1]["total_programa"].as<double>() << "\n";
               cout << "----------------------------------------\n";
           }
       } 

       cout << "\nIngrese los IDs para eliminar la publicidad:\n";
       int programa_id = validarInt("ID Programa: ", 1, 999999);
       int contrato_id = validarInt("ID Contrato: ", 1, 999999);

       bool registroEncontrado = false;
       string nombrePrograma, nombrePatrocinador;
       int segundosPrograma;
       double costoPrograma;

       {
           nontransaction N(c);
           result verify = N.exec_params(
               "SELECT p.nombre as programa, pr.nombre as patrocinador, "
               "pub.segundos_semana, (pub.segundos_semana * p.precio_segundo) as costo "
               "FROM Publicidad pub "
               "JOIN Programas p ON pub.programa_id = p.programa_id "
               "JOIN Patrocinadores pr ON pub.contrato_id = pr.contrato_id "
               "WHERE pub.programa_id = $1 AND pub.contrato_id = $2",
               programa_id, contrato_id
           );
           
           if (!verify.empty()) {
               registroEncontrado = true;
               nombrePrograma = verify[0]["programa"].as<string>();
               nombrePatrocinador = verify[0]["patrocinador"].as<string>();
               segundosPrograma = verify[0]["segundos_semana"].as<int>();
               costoPrograma = verify[0]["costo"].as<double>();
           }
       } 

       if (!registroEncontrado) {
           cout << "\n✘ No se encontró la combinación de programa y patrocinador especificada\n";
           pausar();
           return;
       }

       cout << "\nSe eliminará la siguiente publicidad:\n";
       cout << "----------------------------------------\n";
       cout << "Programa: " << nombrePrograma << "\n";
       cout << "Patrocinador: " << nombrePatrocinador << "\n";
       cout << "Segundos/semana: " << segundosPrograma << "\n";
       cout << "Costo semanal: $" << fixed << setprecision(2) << costoPrograma << "\n";
       
       cout << "\n¿Está seguro de eliminar esta publicidad? (S/N): ";
       char confirma;
       cin >> confirma;
       limpiarBuffer();
       
       if (toupper(confirma) == 'S') {
           work W(c);
           try {
               W.exec_params(
                   "DELETE FROM Publicidad "
                   "WHERE programa_id = $1 AND contrato_id = $2",
                   programa_id, contrato_id
               );
               W.commit();
               cout << "\n✓ Publicidad eliminada exitosamente\n";
           } catch (const exception &e) {
               W.abort();
               throw;
           }
       } else {
           cout << "\nOperación cancelada por el usuario\n";
       }
   } catch (const exception &e) {
       cout << "\n✘ Error: " << e.what() << endl;
   }
   pausar();
}

void consultarPublicidad(connection &c) {
    limpiarConsola();
    cout << "\n=== CONSULTA DE PUBLICIDAD POR PROGRAMA ===\n";
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT p.nombre as programa, pr.nombre as patrocinador, "
            "pub.segundos_semana, (pub.segundos_semana * p.precio_segundo) as costo_semanal "
            "FROM Programas p "
            "JOIN Publicidad pub ON p.programa_id = pub.programa_id "
            "JOIN Patrocinadores pr ON pub.contrato_id = pr.contrato_id "
            "ORDER BY p.programa_id, pr.contrato_id;"
        );

        if (R.empty()) {
            cout << "\nNo hay registros de publicidad\n";
            pausar();
            return;
        }

        string current_program = "";
        double total_program = 0;

        for (const auto &row : R) {
            string programa = row["programa"].as<string>();
            
            if (programa != current_program) {
                if (!current_program.empty()) {
                    cout << "Total programa: $" << fixed << setprecision(2) << total_program << "\n\n";
                }
                current_program = programa;
                total_program = 0;
                cout << "\nPrograma: " << programa << "\n";
                cout << "----------------------------------------\n";
            }
            
            cout << "Patrocinador: " << row["patrocinador"].as<string>() << "\n";
            cout << "Segundos/semana: " << row["segundos_semana"].as<int>() << "\n";
            cout << "Costo semanal: $" << row["costo_semanal"].as<double>() << "\n";
            
            total_program += row["costo_semanal"].as<double>();
        }
        
        if (!current_program.empty()) {
            cout << "Total programa: $" << fixed << setprecision(2) << total_program << "\n";
        }
    } catch (const exception &e) {
        cout << "\n✘ Error: " << e.what() << endl;
    }
    pausar();
}

void mostrarEmisoras(connection &c) {
    limpiarConsola();
    cout << "\n=== EMISORAS REGISTRADAS ===\n";
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT e.nif, e.nombre, e.director, e.provincia, e.banda_hz, "
            "COUNT(DISTINCT ec.cadena_id) as num_cadenas "
            "FROM Emisoras e "
            "LEFT JOIN EmisorasCadena ec ON e.nif = ec.emisora_nif "
            "GROUP BY e.nif, e.nombre, e.director, e.provincia, e.banda_hz "
            "ORDER BY e.nombre;"
        );

        if (R.empty()) {
            cout << "\nNo hay emisoras registradas\n";
        } else {
            for (const auto &row : R) {
                cout << "\n----------------------------------------\n";
                cout << "NIF: " << row["nif"].as<string>() << "\n";
                cout << "Nombre: " << row["nombre"].as<string>() << "\n";
                cout << "Director: " << row["director"].as<string>() << "\n";
                cout << "Provincia: " << row["provincia"].as<string>() << "\n";
                cout << "Banda: " << fixed << setprecision(2) << row["banda_hz"].as<double>() << " MHz\n";
                cout << "Pertenece a " << row["num_cadenas"].as<int>() << " cadena(s)\n";
            }
        }
    } catch (const exception &e) {
        cout << "\n✘ Error: " << e.what() << endl;
    }
    pausar();
}

void menuPrincipal(connection &c) {
    char opcion;
    do {
        limpiarConsola();
        cout << R"(
╭──────────────────────────────────╮
│   GESTIÓN DE PUBLICIDAD RADIAL   │
├──────────────────────────────────┤
│ A) Registrar Emisora             │
│ B) Mostrar Emisoras              │
│ C) Registrar Publicidad          │
│ D) Consultar Publicidad          │
│ E) Eliminar Publicidad           │
│ X) Salir                         │
╰──────────────────────────────────╯
)";
        cout << "\nOpción: ";
        cin >> opcion;
        limpiarBuffer();

        switch (toupper(opcion)) {
            case 'A': registrarEmisora(c); break;
            case 'B': mostrarEmisoras(c); break;
            case 'C': registrarPublicidad(c); break;
            case 'D': consultarPublicidad(c); break;
            case 'E': eliminarPublicidad(c); break;
            case 'X': break;
            default: cout << "\nOpción inválida\n"; pausar();
        }
    } while (toupper(opcion) != 'X');
}

int main() {
    try {
        connection c = conectar();
        comprobarTablas(c);
        menuPrincipal(c);
        return 0;
    } catch (const exception &e) {
        cerr << "Error crítico: " << e.what() << endl;
        return 1;
    }
}