#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <vector>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>

using namespace std;
using namespace pqxx;

// Funciones de utilidad para la interfaz
void limpiarPantalla() {
    system("clear");
}

void limpiarEntrada() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void esperarEntrada() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

void mostrarTitulo(const string& titulo) {
    string linea(50, '=');
    cout << "\n" << linea << endl;
    cout << string((50 - titulo.length()) / 2, ' ') << titulo << endl;
    cout << linea << endl;
}

// Validaciones
bool esNumeroValido(const string& str) {
    if(str.empty()) return false;
    for(char c : str) {
        if(!isdigit(c) && c != '.' && c != '-') return false;
    }
    return true;
}

bool esTextoValido(const string& str, size_t minLen, size_t maxLen) {
    return str.length() >= minLen && str.length() <= maxLen;
}

template<typename T>
T obtenerNumeroValido(const string& mensaje, T min, T max) {
    T numero;
    string entrada;
    bool valido = false;
    
    do {
        cout << mensaje;
        getline(cin, entrada);
        
        stringstream ss(entrada);
        if(ss >> numero && numero >= min && numero <= max) {
            valido = true;
        } else {
            cout << "Error: Ingrese un número entre " << min << " y " << max << endl;
        }
    } while(!valido);
    
    return numero;
}

string obtenerTextoValido(const string& mensaje, size_t minLen, size_t maxLen) {
    string texto;
    bool valido = false;
    
    do {
        cout << mensaje;
        getline(cin, texto);
        
        if(esTextoValido(texto, minLen, maxLen)) {
            valido = true;
        } else {
            cout << "Error: El texto debe tener entre " << minLen << " y " 
                 << maxLen << " caracteres." << endl;
        }
    } while(!valido);
    
    return texto;
}

// Formateo de arrays para PostgreSQL
string formatearArray(const string& elementos) {
    stringstream resultado;
    resultado << "{";
    
    stringstream ss(elementos);
    string item;
    bool primero = true;
    
    while(getline(ss, item, ',')) {
        // Eliminar espacios en blanco
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        if(!item.empty()) {
            if(!primero) resultado << ",";
            resultado << "\"" << item << "\"";
            primero = false;
        }
    }
    
    resultado << "}";
    return resultado.str();
}

// Conexión a la base de datos
connection iniciarConexion() {
    const string CONFIGURACION_DB = 
        "dbname=startrek "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";

    try {
        connection c(CONFIGURACION_DB);
        if (!c.is_open()) {
            throw runtime_error("No se pudo establecer la conexión con la base de datos");
        }
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error al conectar: " + string(e.what()));
        esperarEntrada();
    }
}

// Inicialización de tablas
void inicializarTablas(connection &c) {
    try {
        work W(c);
        
        // Tabla Planetas
        W.exec("CREATE TABLE IF NOT EXISTS Planets ("
               "planet_id SERIAL PRIMARY KEY,"
               "scientific_name VARCHAR(50) UNIQUE NOT NULL,"
               "common_name VARCHAR(100) NOT NULL,"
               "population BIGINT NOT NULL CHECK (population >= 0),"
               "galactic_x NUMERIC(10,2) NOT NULL,"
               "galactic_y NUMERIC(10,2) NOT NULL,"
               "galactic_z NUMERIC(10,2) NOT NULL"
               ");");

        // Tabla Montañas
        W.exec("CREATE TABLE IF NOT EXISTS Mountains ("
               "mountain_id SERIAL PRIMARY KEY,"
               "planet_id INTEGER REFERENCES Planets(planet_id),"
               "name VARCHAR(100) NOT NULL,"
               "height NUMERIC(10,2) NOT NULL CHECK (height > 0)"
               ");");

        // Tabla Imperios
        W.exec("CREATE TABLE IF NOT EXISTS Empires ("
               "empire_id SERIAL PRIMARY KEY,"
               "galactic_code VARCHAR(20) UNIQUE NOT NULL,"
               "name VARCHAR(100) NOT NULL,"
               "avg_temperature NUMERIC(6,2) NOT NULL"
               ");");

        // Tabla PlanetasImperios
        W.exec("CREATE TABLE IF NOT EXISTS Planet_Empires ("
               "planet_id INTEGER REFERENCES Planets(planet_id),"
               "empire_id INTEGER REFERENCES Empires(empire_id),"
               "PRIMARY KEY (planet_id)"  // Un planeta solo puede pertenecer a un imperio
               ");");

        // Tabla Razas
        W.exec("CREATE TABLE IF NOT EXISTS Races ("
               "race_id SERIAL PRIMARY KEY,"
               "scientific_name VARCHAR(50) UNIQUE NOT NULL"
               ");");

        // Tabla Habilidades de Razas
        W.exec("CREATE TABLE IF NOT EXISTS Race_Skills ("
               "race_id INTEGER REFERENCES Races(race_id),"
               "skill_description TEXT NOT NULL,"
               "PRIMARY KEY (race_id, skill_description)"
               ");");

        // Tabla PlanetasRazas (con porcentaje de población)
        W.exec("CREATE TABLE IF NOT EXISTS Planet_Races ("
               "planet_id INTEGER REFERENCES Planets(planet_id),"
               "race_id INTEGER REFERENCES Races(race_id),"
               "population_percentage NUMERIC(5,2) NOT NULL CHECK (population_percentage BETWEEN 0 AND 100),"
               "PRIMARY KEY (planet_id, race_id)"
               ");");

        // Tabla Flotas
        W.exec("CREATE TABLE IF NOT EXISTS Fleets ("
               "fleet_id SERIAL PRIMARY KEY,"
               "galactic_code VARCHAR(20) UNIQUE NOT NULL,"
               "destination VARCHAR(100) NOT NULL,"
               "empire_id INTEGER REFERENCES Empires(empire_id) NOT NULL"
               ");");

        // Tabla Misiones de Flotas
        W.exec("CREATE TABLE IF NOT EXISTS Fleet_Missions ("
               "fleet_id INTEGER REFERENCES Fleets(fleet_id),"
               "mission_type VARCHAR(50) NOT NULL,"
               "PRIMARY KEY (fleet_id, mission_type)"
               ");");

        // Tabla Capitanes
        W.exec("CREATE TABLE IF NOT EXISTS Captains ("
               "captain_id SERIAL PRIMARY KEY,"
               "identification_code VARCHAR(20) UNIQUE NOT NULL,"
               "name VARCHAR(100) NOT NULL,"
               "birth_planet_id INTEGER REFERENCES Planets(planet_id),"
               "empire_id INTEGER REFERENCES Empires(empire_id)"
               ");");

        // Tabla Maniobras
        W.exec("CREATE TABLE IF NOT EXISTS Maneuvers ("
               "maneuver_id SERIAL PRIMARY KEY,"
               "name VARCHAR(100) UNIQUE NOT NULL,"
               "energy_consumption INTEGER NOT NULL CHECK (energy_consumption > 0)"
               ");");

        // Tabla Naves
        W.exec("CREATE TABLE IF NOT EXISTS Ships ("
               "ship_id SERIAL PRIMARY KEY,"
               "fleet_code VARCHAR(20) NOT NULL,"
               "max_speed NUMERIC(10,2) NOT NULL CHECK (max_speed > 0),"
               "energy INTEGER NOT NULL CHECK (energy >= 0),"
               "fleet_id INTEGER REFERENCES Fleets(fleet_id),"
               "captain_id INTEGER REFERENCES Captains(captain_id) UNIQUE,"  // Un capitán solo puede tener una nave
               "UNIQUE(fleet_id, fleet_code)"  // Código único dentro de la flota
               ");");

        // Tabla Naves-Maniobras
        W.exec("CREATE TABLE IF NOT EXISTS Ship_Maneuvers ("
               "ship_id INTEGER REFERENCES Ships(ship_id),"
               "maneuver_id INTEGER REFERENCES Maneuvers(maneuver_id),"
               "PRIMARY KEY (ship_id, maneuver_id)"
               ");");

        W.commit();
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

// Funciones de registro
void registrarPlaneta(connection &c) {
    limpiarPantalla();
    mostrarTitulo("REGISTRO DE NUEVO PLANETA");
    
    try {
        string nombre_cientifico = obtenerTextoValido("Nombre científico (5-50 caracteres): ", 5, 50);
        string nombre_comun = obtenerTextoValido("Nombre común (3-100 caracteres): ", 3, 100);
        long long poblacion = obtenerNumeroValido<long long>("Población total: ", 0, 9999999999999LL);
        double coord_x = obtenerNumeroValido<double>("Coordenada X: ", -999999.99, 999999.99);
        double coord_y = obtenerNumeroValido<double>("Coordenada Y: ", -999999.99, 999999.99);
        double coord_z = obtenerNumeroValido<double>("Coordenada Z: ", -999999.99, 999999.99);

        work W(c);
        W.exec_params("INSERT INTO Planets (scientific_name, common_name, population, "
                     "galactic_x, galactic_y, galactic_z) VALUES ($1, $2, $3, $4, $5, $6)",
                     nombre_cientifico, nombre_comun, poblacion, coord_x, coord_y, coord_z);
        W.commit();

        cout << "\nPlaneta registrado exitosamente." << endl;
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}

void registrarImperio(connection &c) {
    limpiarPantalla();
    mostrarTitulo("REGISTRO DE NUEVO IMPERIO");
    
    try {
        string codigo = obtenerTextoValido("Código galáctico (5-20 caracteres): ", 5, 20);
        string nombre = obtenerTextoValido("Nombre del imperio (3-100 caracteres): ", 3, 100);
        double temperatura = obtenerNumeroValido<double>("Temperatura promedio (-273.15 a 1000.00): ", -273.15, 1000.00);

        work W(c);
        W.exec_params("INSERT INTO Empires (galactic_code, name, avg_temperature) "
                     "VALUES ($1, $2, $3)",
                     codigo, nombre, temperatura);
        W.commit();

        cout << "\nImperio registrado exitosamente." << endl;
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}

void registrarFlota(connection &c) {
    limpiarPantalla();
    mostrarTitulo("REGISTRO DE NUEVA FLOTA");
    
    try {
        // Bloque para consultar imperios disponibles
        vector<pair<int, string>> imperios;
        {
            nontransaction N(c);
            result R = N.exec("SELECT empire_id, name FROM Empires ORDER BY name;");
            
            if (R.empty()) {
                cout << "No hay imperios registrados. Primero debe registrar un imperio." << endl;
                esperarEntrada();
                return;
            }

            cout << "Imperios disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                imperios.push_back(make_pair(row[0].as<int>(), row[1].as<string>()));
                cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
            }
        } // La nontransaction se cierra aquí

        int imperio_id = obtenerNumeroValido<int>("\nSeleccione el ID del imperio: ", 1, 999999);
        string codigo = obtenerTextoValido("Código galáctico de la flota (5-20 caracteres): ", 5, 20);
        string destino = obtenerTextoValido("Destino de la flota (3-100 caracteres): ", 3, 100);
        
        // Verificar que el imperio existe
        bool imperio_encontrado = false;
        for (const auto& imp : imperios) {
            if (imp.first == imperio_id) {
                imperio_encontrado = true;
                break;
            }
        }
        
        if (!imperio_encontrado) {
            cout << "\nError: El ID del imperio seleccionado no existe." << endl;
            esperarEntrada();
            return;
        }
        
        work W(c);
        
        // Insertar la flota
        W.exec_params("INSERT INTO Fleets (galactic_code, destination, empire_id) "
                     "VALUES ($1, $2, $3)",
                     codigo, destino, imperio_id);

        // Registrar misiones
        cout << "\nRegistro de misiones (escriba 'fin' para terminar):" << endl;
        string mision;
        while (true) {
            mision = obtenerTextoValido("Ingrese una misión (3-50 caracteres): ", 3, 50);
            if (mision == "fin") break;
            
            W.exec_params("INSERT INTO Fleet_Missions (fleet_id, mission_type) "
                         "SELECT currval('fleets_fleet_id_seq'), $1",
                         mision);
        }

        W.commit();
        cout << "\nFlota registrada exitosamente." << endl;
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}
// Funciones de consulta
void consultarPlanetas(connection &c) {
    limpiarPantalla();
    mostrarTitulo("CONSULTA DE PLANETAS");
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT p.scientific_name, p.common_name, p.population, "
            "e.name as empire_name, "
            "string_agg(DISTINCT m.name || ' (' || m.height || 'm)', ', ') as mountains "
            "FROM Planets p "
            "LEFT JOIN Planet_Empires pe ON p.planet_id = pe.planet_id "
            "LEFT JOIN Empires e ON pe.empire_id = e.empire_id "
            "LEFT JOIN Mountains m ON p.planet_id = m.planet_id "
            "GROUP BY p.planet_id, p.scientific_name, p.common_name, p.population, e.name "
            "ORDER BY p.scientific_name;"
        );

        if (R.empty()) {
            cout << "No hay planetas registrados en el sistema." << endl;
        } else {
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << "\nPlaneta: " << row["scientific_name"].as<string>() 
                   << " (" << row["common_name"].as<string>() << ")" << endl;
                cout << "Población: " << row["population"].as<string>() << endl;
                cout << "Imperio: " << (row["empire_name"].is_null() ? "Sin imperio" : row["empire_name"].as<string>()) << endl;
                cout << "Montañas: " << (row["mountains"].is_null() ? "Sin montañas registradas" : row["mountains"].as<string>()) << endl;
                cout << "------------------------" << endl;
            }
        }
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}

void consultarFlotas(connection &c) {
    limpiarPantalla();
    mostrarTitulo("CONSULTA DE FLOTAS");
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT f.fleet_id, f.galactic_code, f.destination, "
            "e.name as empire_name, "
            "string_agg(DISTINCT fm.mission_type, ', ') as missions, "
            "COUNT(DISTINCT s.ship_id) as ship_count "
            "FROM Fleets f "
            "JOIN Empires e ON f.empire_id = e.empire_id "
            "LEFT JOIN Fleet_Missions fm ON f.fleet_id = fm.fleet_id "
            "LEFT JOIN Ships s ON f.fleet_id = s.fleet_id "
            "GROUP BY f.fleet_id, f.galactic_code, f.destination, e.name "
            "ORDER BY e.name, f.galactic_code;"
        );

        if (R.empty()) {
            cout << "No hay flotas registradas en el sistema." << endl;
        } else {
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << "\nFlota: " << row["galactic_code"].as<string>() << endl;
                cout << "Imperio: " << row["empire_name"].as<string>() << endl;
                cout << "Destino: " << row["destination"].as<string>() << endl;
                cout << "Misiones: " << row["missions"].as<string>() << endl;
                cout << "Naves asignadas: " << row["ship_count"].as<int>() << endl;
                cout << "------------------------" << endl;
            }
        }
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}

void consultarImperios(connection &c) {
    limpiarPantalla();
    mostrarTitulo("CONSULTA DE IMPERIOS");
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT e.empire_id, e.galactic_code, e.name, e.avg_temperature, "
            "COUNT(DISTINCT p.planet_id) as planet_count, "
            "COUNT(DISTINCT f.fleet_id) as fleet_count "
            "FROM Empires e "
            "LEFT JOIN Planet_Empires pe ON e.empire_id = pe.empire_id "
            "LEFT JOIN Planets p ON pe.planet_id = p.planet_id "
            "LEFT JOIN Fleets f ON e.empire_id = f.empire_id "
            "GROUP BY e.empire_id, e.galactic_code, e.name, e.avg_temperature "
            "ORDER BY e.name;"
        );

        if (R.empty()) {
            cout << "No hay imperios registrados en el sistema." << endl;
        } else {
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << "\nImperio: " << row["name"].as<string>() << endl;
                cout << "Código Galáctico: " << row["galactic_code"].as<string>() << endl;
                cout << "Temperatura Promedio: " << row["avg_temperature"].as<double>() << "°C" << endl;
                cout << "Planetas controlados: " << row["planet_count"].as<int>() << endl;
                cout << "Flotas activas: " << row["fleet_count"].as<int>() << endl;
                cout << "------------------------" << endl;
            }
        }
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    esperarEntrada();
}

// Menú principal
void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarPantalla();
        mostrarTitulo("JUEGO STAR TREK");
        cout << "\n[1] Gestión de Planetas" << endl;
        cout << "    - Registrar nuevo planeta" << endl;
        cout << "    - Consultar planetas existentes" << endl;
        cout << "\n[2] Gestión de Imperios" << endl;
        cout << "    - Registrar nuevo imperio" << endl;
        cout << "    - Consultar imperios y sus dominios" << endl;
        cout << "\n[3] Gestión de Flotas" << endl;
        cout << "    - Registrar nueva flota" << endl;
        cout << "    - Consultar flotas y sus misiones" << endl;
        cout << "\n[0] Salir del Sistema" << endl;

        opcion = obtenerNumeroValido<int>("\nSeleccione una opción: ", 0, 3);

        try {
            switch (opcion) {
                case 1: {
                    limpiarPantalla();
                    int subopcion = obtenerNumeroValido<int>(
                        "\n1. Registrar Planeta\n2. Consultar Planetas\nSeleccione: ", 1, 2);
                    if (subopcion == 1) registrarPlaneta(c);
                    else consultarPlanetas(c);
                    break;
                }
                case 2: {
                    limpiarPantalla();
                    int subopcion = obtenerNumeroValido<int>(
                        "\n1. Registrar Imperio\n2. Consultar Imperios\nSeleccione: ", 1, 2);
                    if (subopcion == 1) registrarImperio(c);
                    else consultarImperios(c);
                    break;
                }
                case 3: {
                    limpiarPantalla();
                    int subopcion = obtenerNumeroValido<int>(
                        "\n1. Registrar Flota\n2. Consultar Flotas\nSeleccione: ", 1, 2);
                    if (subopcion == 1) registrarFlota(c);
                    else consultarFlotas(c);
                    break;
                }
                case 0:
                    limpiarPantalla();
                    cout << "Finalizando el sistema..." << endl;
                    break;
            }
        } catch (const exception &e) {
            cerr << "\nERROR en la operación: " << e.what() << endl;
            esperarEntrada();
        }
    } while (opcion != 0);
}

int main() {
    limpiarPantalla();
    mostrarTitulo("INICIALIZANDO SISTEMA <<JUEGO STAR TREK>>");
    
    try {
        cout << "\nConectando con la base de datos..." << endl;
        connection c = iniciarConexion();
        
        cout << "Verificando estructura de datos..." << endl;
        inicializarTablas(c);
        
        cout << "Sistema inicializado correctamente" << endl;
        system("sleep 1");
        
        esperarEntrada();
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << "\nError crítico: " << e.what() << endl;
        cout << "\nEl sistema se cerrará debido a un error." << endl;
        esperarEntrada();
        return 1;
    }
    
    cout << "\nGracias por usar el Sistema Star Trek" << endl;
    system("sleep 1");
    return 0;
}