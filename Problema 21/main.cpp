#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <limits>
#include <vector>
#include <regex>

using namespace std;
using namespace pqxx;

// Funciones de utilidad
void limpiarTerminal() {
    system("clear");
}

void limpiarEntrada() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausarSistema() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

// Funciones auxiliares de validación
// Función de validación de documento de identidad (alphanumeric, 6-15 caracteres)
bool validarDocumentoIdentidad(const string& documento) {
    // Verificar longitud (entre 6 y 15 caracteres)
    if (documento.length() < 6 || documento.length() > 15) {
        return false;
    }

    // Verificar que solo contenga caracteres alfanuméricos
    return all_of(documento.begin(), documento.end(), [](char c) {
        return isalnum(c);
    });
}

// Función de validación de nombre (sin números ni caracteres especiales)
bool validarNombre(const string& nombre) {
    // Permitir espacios y letras (incluyendo acentos)
    regex patron("^[A-Za-zÁÉÍÓÚáéíóúÑñ ]+$");
    return regex_match(nombre, patron) && 
           nombre.length() >= 2 && 
           nombre.length() <= 100;
}

// Función de validación de fecha de partido
bool validarFechaPartido(const string& fecha_str) {
    // Verificar formato YYYY-MM-DD y rango de fechas del Mundial
    regex patron("^(2014)-(0[1-7])-(0[1-9]|[12][0-9]|3[01])$");
    
    if (!regex_match(fecha_str, patron)) return false;
    
    // Convertir a fecha
    int anio, mes, dia;
    sscanf(fecha_str.c_str(), "%d-%d-%d", &anio, &mes, &dia);
    
    // Validar rango específico del Mundial Brasil 2014
    return (anio == 2014 && mes >= 6 && mes <= 7 && dia >= 1 && dia <= 31);
}

// Función para validar domicilio
bool validarDomicilio(const string& domicilio) {
    // Permitir números, letras, espacios y algunos caracteres de dirección
    regex patron("^[A-Za-zÁÉÍÓÚáéíóúÑñ0-9 ,.-]+$");
    return regex_match(domicilio, patron) && 
           domicilio.length() >= 5 && 
           domicilio.length() <= 300;
}

// Función para validar número de mundiales ganados
bool validarMundiales(int mundiales) {
    return mundiales >= 0 && mundiales <= 5; // Máximo 5 mundiales históricamente
}

bool verificarLimitePacks(connection &c, int hincha_id) {
    nontransaction N(c);
    
    // Contar packs únicos por hincha
    string query = "SELECT COUNT(DISTINCT pack_id) FROM PacksHinchas WHERE hincha_id = $1";
    result R = N.exec_params(query, hincha_id);
    
    int total_packs = R[0][0].as<int>();
    
    // Límite de 3 packs diferentes por hincha
    return total_packs < 3;
}

// Función para validar cantidad de partidos por equipo
bool validarCantidadPartidosPorEquipo(connection &c, int equipo_id) {
    nontransaction N(c);
    
    // Consultar cantidad de partidos jugados por el equipo
    string query = "SELECT COUNT(*) FROM ("
        "SELECT partido_id FROM Partidos "
        "WHERE equipo1_id = $1 OR equipo2_id = $1"
        ") AS partidos_jugados";
    
    result R = N.exec_params(query, equipo_id);
    int partidos_jugados = R[0][0].as<int>();

    // Modificar la función registrarPartido para usar esta validación
    result R_equipo = N.exec_params(
        "SELECT mundiales_ganados FROM Equipos WHERE equipo_id = $1", 
        equipo_id
    );
    int mundiales_ganados = R_equipo[0][0].as<int>();

    // Definir límites de partidos según historial de mundiales
    int max_partidos;
    switch(mundiales_ganados) {
        case 0: max_partidos = 3;  // Equipos sin historia de campeonato
        case 1: max_partidos = 5;  // Equipos con poca historia
        case 2: max_partidos = 6;  // Equipos con más experiencia
        default: max_partidos = 7; // Equipos con múltiples títulos
    }

    return partidos_jugados < max_partidos;
}

// Función para establecer conexión con la base de datos
connection iniciarConexionBaseDatos() {
    const string CONFIGURACION_DB = 
        "dbname=mundial2014 "
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
}

// Función para crear las tablas del sistema
void configurarEstructuraBD(connection &c) {
    try {
        work W(c);

        // Tabla de Equipos
        string query_equipos = "CREATE TABLE IF NOT EXISTS Equipos ("
            "equipo_id SERIAL PRIMARY KEY, "
            "pais VARCHAR(100) UNIQUE, "
            "sede_hospedaje VARCHAR(200), "
            "mundiales_ganados INTEGER, "
            "director_tecnico VARCHAR(100)"
        ");";
        W.exec(query_equipos);

        // Tabla de Hinchas
        string query_hinchas = "CREATE TABLE IF NOT EXISTS Hinchas ("
            "hincha_id SERIAL PRIMARY KEY, "
            "nombre VARCHAR(200), "
            "dni VARCHAR(20) UNIQUE, "
            "domicilio VARCHAR(300), "
            "nacionalidad VARCHAR(100), "
            "invitado_por INTEGER REFERENCES Hinchas(hincha_id)"
        ");";
        W.exec(query_hinchas);

        // Tabla de Hoteles
        string query_hoteles = "CREATE TABLE IF NOT EXISTS Hoteles ("
            "hotel_id VARCHAR(50) PRIMARY KEY, "
            "nombre VARCHAR(200), "
            "direccion VARCHAR(300), "
            "estrellas INTEGER"
        ");";
        W.exec(query_hoteles);

        // Tabla de Hospedaje (Relación Hinchas-Hoteles)
        string query_hospedaje = "CREATE TABLE IF NOT EXISTS Hospedaje ("
            "hincha_id INTEGER REFERENCES Hinchas(hincha_id), "
            "hotel_id VARCHAR(50) REFERENCES Hoteles(hotel_id), "
            "PRIMARY KEY (hincha_id, hotel_id)"
        ");";
        W.exec(query_hospedaje);

        // Tabla de Packs de Cotillón
        string query_packs = "CREATE TABLE IF NOT EXISTS PacksCotillon ("
            "pack_id SERIAL PRIMARY KEY, "
            "nombre VARCHAR(200), "
            "elementos TEXT[]"
        ");";
        W.exec(query_packs);

        // Tabla de Packs de Hinchas
        string query_packs_hinchas = "CREATE TABLE IF NOT EXISTS PacksHinchas ("
            "hincha_id INTEGER REFERENCES Hinchas(hincha_id), "
            "pack_id INTEGER REFERENCES PacksCotillon(pack_id), "
            "PRIMARY KEY (hincha_id, pack_id)"
        ");";
        W.exec(query_packs_hinchas);

        // Tabla de Estadios
        string query_estadios = "CREATE TABLE IF NOT EXISTS Estadios ("
            "estadio_id SERIAL PRIMARY KEY, "
            "nombre VARCHAR(200)"
        ");";
        W.exec(query_estadios);

        // Tabla de Partidos
        string query_partidos = "CREATE TABLE IF NOT EXISTS Partidos ("
            "partido_id SERIAL PRIMARY KEY, "
            "fecha DATE, "
            "estadio_id INTEGER REFERENCES Estadios(estadio_id), "
            "espectadores INTEGER, "
            "equipo1_id INTEGER REFERENCES Equipos(equipo_id), "
            "equipo2_id INTEGER REFERENCES Equipos(equipo_id), "
            "etapa VARCHAR(20) CHECK (etapa IN ('Grupos', 'Octavos', 'Cuartos', 'Semifinal', 'Final')) "
        ");";
        W.exec(query_partidos);

        // Tabla de Árbitros
        string query_arbitros = "CREATE TABLE IF NOT EXISTS Arbitros ("
            "arbitro_id SERIAL PRIMARY KEY, "
            "nombre VARCHAR(200)"
        ");";
        W.exec(query_arbitros);

        // Tabla de Árbitros por Partido
        string query_arbitros_partidos = "CREATE TABLE IF NOT EXISTS ArbitrosPartidos ("
            "partido_id INTEGER REFERENCES Partidos(partido_id), "
            "arbitro_id INTEGER REFERENCES Arbitros(arbitro_id), "
            "PRIMARY KEY (partido_id, arbitro_id)"
        ");";
        W.exec(query_arbitros_partidos);

        // Tabla de Fun Fests
        string query_fun_fests = "CREATE TABLE IF NOT EXISTS FunFests ("
            "funfest_id VARCHAR(50) PRIMARY KEY, "
            "ciudad VARCHAR(200), "
            "duracion INTERVAL, "
            "partido_id INTEGER REFERENCES Partidos(partido_id)"
        ");";
        W.exec(query_fun_fests);

        // Tabla de Consumo de Cerveza en Partidos
        string query_consumo_cerveza = "CREATE TABLE IF NOT EXISTS ConsumoCerveza ("
            "hincha_id INTEGER REFERENCES Hinchas(hincha_id), "
            "partido_id INTEGER REFERENCES Partidos(partido_id), "
            "cantidad_cervezas INTEGER, "
            "PRIMARY KEY (hincha_id, partido_id)"
        ");";
        W.exec(query_consumo_cerveza);

        W.commit();
        cout << "Estructura de base de datos inicializada correctamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante la configuración de la base de datos: " << e.what() << endl;
        throw;
    }

}

// Función para registrar un nuevo hincha
void registrarHincha(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string nombre, dni, domicilio, nacionalidad;
        int hincha_invitador = 0;

        cout << "=== REGISTRO DE HINCHA ===" << endl;
        
        // Validación de nombre
        do {
            cout << "Nombre completo: ";
            getline(cin, nombre);
            
            if (!validarNombre(nombre)) {
                cout << "Nombre inválido. Use solo letras y espacios (2-100 caracteres)." << endl;
            }
        } while (!validarNombre(nombre));

        do {
            cout << "Documento de Identidad: ";
            getline(cin, dni);
            
            if (!validarDocumentoIdentidad(dni)) {
                cout << "Documento inválido. Debe contener 6-15 caracteres alfanuméricos." << endl;
            }
        } while (!validarDocumentoIdentidad(dni));

        // Validación de domicilio
        do {
            cout << "Domicilio completo: ";
            getline(cin, domicilio);
            
            if (!validarDomicilio(domicilio)) {
                cout << "Domicilio inválido. Use letras, números y caracteres de dirección válidos." << endl;
            }
        } while (!validarDomicilio(domicilio));

        // Validación de nacionalidad
        do {
            cout << "Nacionalidad: ";
            getline(cin, nacionalidad);
            
            if (!validarNombre(nacionalidad)) {
                cout << "Nacionalidad inválida. Use solo letras." << endl;
            }
        } while (!validarNombre(nacionalidad));

        // Resto del código de registro de hincha...
    } catch (const exception &e) {
        cerr << "Error durante el registro del hincha: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar un nuevo hotel
void registrarHotel(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string hotel_id, nombre, direccion;
        int estrellas;

        cout << "=== REGISTRO DE HOTEL ===" << endl;
        
        cout << "ID del hotel (único a nivel nacional): ";
        getline(cin, hotel_id);

        cout << "Nombre del hotel: ";
        getline(cin, nombre);

        cout << "Dirección completa: ";
        getline(cin, direccion);

        // Validación de estrellas
        do {
            cout << "Número de estrellas (1-5): ";
            cin >> estrellas;
            limpiarEntrada();
        } while (estrellas < 1 || estrellas > 5);

        // Verificar si el hotel ya existe
        result R = W.exec_params("SELECT * FROM Hoteles WHERE hotel_id = $1", hotel_id);
        if (!R.empty()) {
            cout << "El hotel con este ID ya existe. Actualice la información." << endl;
            pausarSistema();
            return;
        }

        // Insertar nuevo hotel
        string query = "INSERT INTO Hoteles (hotel_id, nombre, direccion, estrellas) "
                       "VALUES ($1, $2, $3, $4);";
        
        W.exec_params(query, hotel_id, nombre, direccion, estrellas);
        W.commit();

        cout << "\nHotel registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del hotel: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar un nuevo equipo
void registrarEquipo(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string pais, sede_hospedaje, director_tecnico;
        int mundiales_ganados;

        cout << "=== REGISTRO DE EQUIPO ===" << endl;
        
        // Validación de país
        do {
            cout << "País del equipo: ";
            getline(cin, pais);
            
            if (!validarNombre(pais)) {
                cout << "País inválido. Use solo letras." << endl;
            }
        } while (!validarNombre(pais));

        // Validación de sede de hospedaje
        do {
            cout << "Sede de hospedaje: ";
            getline(cin, sede_hospedaje);
            
            if (!validarDomicilio(sede_hospedaje)) {
                cout << "Sede de hospedaje inválida." << endl;
            }
        } while (!validarDomicilio(sede_hospedaje));

        // Validación de nombre del Director Técnico
        do {
            cout << "Nombre del Director Técnico: ";
            getline(cin, director_tecnico);
            
            if (!validarNombre(director_tecnico)) {
                cout << "Nombre de Director Técnico inválido." << endl;
            }
        } while (!validarNombre(director_tecnico));

        // Validación de mundiales ganados
        do {
            cout << "Cantidad de mundiales ganados: ";
            cin >> mundiales_ganados;
            limpiarEntrada();
            
            if (!validarMundiales(mundiales_ganados)) {
                cout << "Cantidad de mundiales inválida (0-5)." << endl;
            }
        } while (!validarMundiales(mundiales_ganados));

        // Verificar si el equipo ya existe
        result R = W.exec_params("SELECT * FROM Equipos WHERE pais = $1", pais);
        if (!R.empty()) {
            cout << "El equipo de este país ya está registrado. Actualice la información." << endl;
            pausarSistema();
            return;
        }

        // Insertar nuevo equipo
        string query = "INSERT INTO Equipos (pais, sede_hospedaje, mundiales_ganados, director_tecnico) "
                       "VALUES ($1, $2, $3, $4);";
        
        W.exec_params(query, pais, sede_hospedaje, mundiales_ganados, director_tecnico);
        W.commit();

        cout << "\nEquipo registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del equipo: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar un partido
void registrarPartido(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string fecha_str, nombre_estadio, etapa;
        int espectadores, equipo1_id, equipo2_id, estadio_id;
        vector<string> nombres_arbitros;

        cout << "=== REGISTRO DE PARTIDO ===" << endl;
        
        // Seleccionar equipos
        {
            nontransaction N(c);
            result R = N.exec("SELECT equipo_id, pais FROM Equipos ORDER BY pais;");
            
            cout << "Equipos disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
            }
        }

        // Validación de selección de equipos
        do {
            cout << "Seleccione ID del primer equipo: ";
            cin >> equipo1_id;
            limpiarEntrada();

            cout << "Seleccione ID del segundo equipo: ";
            cin >> equipo2_id;
            limpiarEntrada();

            if (equipo1_id == equipo2_id) {
                cout << "Error: Debe seleccionar dos equipos diferentes." << endl;
                continue;
            }

            // Validar existencia de equipos
            nontransaction N(c);
            result R1 = N.exec_params("SELECT 1 FROM Equipos WHERE equipo_id = $1", equipo1_id);
            result R2 = N.exec_params("SELECT 1 FROM Equipos WHERE equipo_id = $1", equipo2_id);

            if (R1.empty() || R2.empty()) {
                cout << "Error: Uno o ambos equipos no existen." << endl;
                continue;
            }

            break;
        } while (true);

        // Validación de fecha de partido
        do {
            cout << "Fecha del partido (YYYY-MM-DD): ";
            getline(cin, fecha_str);
            
            if (!validarFechaPartido(fecha_str)) {
                cout << "Fecha inválida. Debe estar en el rango del Mundial Brasil 2014 (junio-julio)." << endl;
            }
        } while (!validarFechaPartido(fecha_str));

        // Validación de cantidad de partidos por equipo
        {
            nontransaction N(c);
            
            auto verificarPartidosEquipo = [&](int equipo_id) -> bool {
                result R = N.exec_params(
                    "SELECT mundiales_ganados, "
                    "(SELECT COUNT(*) FROM Partidos WHERE equipo1_id = $1 OR equipo2_id = $1) AS partidos_jugados "
                    "FROM Equipos WHERE equipo_id = $1", 
                    equipo_id
                );

                int mundiales = R[0][0].as<int>();
                int partidos = R[0][1].as<int>();

                int max_partidos;
                switch(mundiales) {
                    case 0: max_partidos = 3; break;
                    case 1: max_partidos = 4; break;
                    case 2: max_partidos = 5; break;
                    case 3: max_partidos = 6; break;
                    default: max_partidos = 7; break;
                }

                return partidos < max_partidos;
            };

            if (!verificarPartidosEquipo(equipo1_id) || !verificarPartidosEquipo(equipo2_id)) {
                cout << "Error: Uno de los equipos ha alcanzado su límite de partidos en este Mundial." << endl;
                pausarSistema();
                return;
            }
        }

        // Registro de estadio
        do {
            cout << "Nombre del estadio: ";
            getline(cin, nombre_estadio);
            
            if (nombre_estadio.length() < 3 || nombre_estadio.length() > 100) {
                cout << "Nombre de estadio inválido (3-100 caracteres)." << endl;
                continue;
            }

            // Verificar o crear estadio
            nontransaction N(c);
            result R = N.exec_params("SELECT estadio_id FROM Estadios WHERE nombre = $1", nombre_estadio);
            
            if (R.empty()) {
                work W1(c);
                result R_nuevo = W1.exec_params("INSERT INTO Estadios (nombre) VALUES ($1) RETURNING estadio_id", nombre_estadio);
                estadio_id = R_nuevo[0][0].as<int>();
                W1.commit();
            } else {
                estadio_id = R[0][0].as<int>();
            }
            
            break;
        } while (true);

        // Validación de número de espectadores
        do {
            cout << "Número de espectadores: ";
            cin >> espectadores;
            limpiarEntrada();
            
            if (espectadores < 0 || espectadores > 100000) {
                cout << "Número de espectadores inválido (0-100,000)." << endl;
            }
        } while (espectadores < 0 || espectadores > 100000);

        // Selección de etapa
        do {
            cout << "Etapa del partido:" << endl;
            cout << "1. Grupos" << endl;
            cout << "2. Octavos" << endl;
            cout << "3. Cuartos" << endl;
            cout << "4. Semifinal" << endl;
            cout << "5. Final" << endl;
            cout << "Seleccione una opción: ";
            
            int opcion_etapa;
            cin >> opcion_etapa;
            limpiarEntrada();

            switch(opcion_etapa) {
                case 1: etapa = "Grupos"; break;
                case 2: etapa = "Octavos"; break;
                case 3: etapa = "Cuartos"; break;
                case 4: etapa = "Semifinal"; break;
                case 5: etapa = "Final"; break;
                default: 
                    cout << "Opción inválida. Intente nuevamente." << endl;
                    continue;
            }
            break;
        } while(true);

        // Registro de árbitros
        cout << "\nRegistro de Árbitros:" << endl;
        do {
            string nombre_arbitro;
            cout << "Nombre del árbitro (o 'fin' para terminar): ";
            getline(cin, nombre_arbitro);
            
            if (nombre_arbitro == "fin") break;
            
            if (!validarNombre(nombre_arbitro)) {
                cout << "Nombre de árbitro inválido." << endl;
                continue;
            }
            
            nombres_arbitros.push_back(nombre_arbitro);
        } while (true);

        // Insertar partido
        work W2(c);
        string query_partido = "INSERT INTO Partidos (fecha, estadio_id, espectadores, equipo1_id, equipo2_id, etapa) "
                                "VALUES ($1, $2, $3, $4, $5, $6) RETURNING partido_id;";
        
        result R_partido = W2.exec_params(query_partido, fecha_str, estadio_id, espectadores, 
                                           equipo1_id, equipo2_id, etapa);
        int partido_id = R_partido[0][0].as<int>();

        // Registrar árbitros
        for (const auto& arbitro : nombres_arbitros) {
            // Insertar árbitro si no existe
            string query_arbitro = "INSERT INTO Arbitros (nombre) VALUES ($1) "
                                   "ON CONFLICT (nombre) DO NOTHING RETURNING arbitro_id;";
            result R_arbitro = W2.exec_params(query_arbitro, arbitro);
            
            int arbitro_id;
            if (R_arbitro.empty()) {
                // Si ya existe, obtener su ID
                nontransaction N(c);
                result R = N.exec_params("SELECT arbitro_id FROM Arbitros WHERE nombre = $1", arbitro);
                arbitro_id = R[0][0].as<int>();
            } else {
                arbitro_id = R_arbitro[0][0].as<int>();
            }

            // Asociar árbitro con partido
            string query_arbitro_partido = "INSERT INTO ArbitrosPartidos (partido_id, arbitro_id) "
                                           "VALUES ($1, $2);";
            W2.exec_params(query_arbitro_partido, partido_id, arbitro_id);
        }

        W2.commit();

        cout << "\nPartido registrado exitosamente" << endl;
        cout << "Partido #" << partido_id << ": " << etapa << " - Espectadores: " << espectadores << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del partido: " << e.what() << endl;
    }
    pausarSistema();
}

void consultarPartidosPorEtapa(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT etapa, COUNT(*) as partidos, "
            "array_agg(DISTINCT e1.pais || ' vs ' || e2.pais) AS enfrentamientos "
            "FROM Partidos p "
            "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
            "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id "
            "GROUP BY etapa "
            "ORDER BY partidos DESC;"
        );
        
        cout << "=== PARTIDOS POR ETAPA ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nEtapa: " << row[0].as<string>() << endl;
            cout << "Número de partidos: " << row[1].as<int>() << endl;
            
            string enfrentamientos = row[2].as<string>();
            cout << "Enfrentamientos: " << enfrentamientos.substr(1, enfrentamientos.length() - 2) << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar partidos por etapa: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar un Fun Fest
void registrarFunFest(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string funfest_id, ciudad, duracion_str;
        int partido_id;

        cout << "=== REGISTRO DE FUN FEST ===" << endl;
        
        // Mostrar partidos disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT p.partido_id, e1.pais AS equipo1, e2.pais AS equipo2, p.fecha "
                              "FROM Partidos p "
                              "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
                              "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id;");
            
            cout << "Partidos disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " 
                     << row[1].as<string>() << " vs " 
                     << row[2].as<string>() << " (" 
                     << row[3].as<string>() << ")" << endl;
            }
        }

        cout << "Seleccione el ID del partido a transmitir: ";
        cin >> partido_id;
        limpiarEntrada();

        cout << "Código de Fun Fest: ";
        getline(cin, funfest_id);

        cout << "Ciudad: ";
        getline(cin, ciudad);

        cout << "Duración (formato HH:MM:SS): ";
        getline(cin, duracion_str);

        // Insertar Fun Fest
        string query = "INSERT INTO FunFests (funfest_id, ciudad, duracion, partido_id) "
                       "VALUES ($1, $2, $3, $4);";
        
        W.exec_params(query, funfest_id, ciudad, duracion_str, partido_id);
        W.commit();

        cout << "\nFun Fest registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del Fun Fest: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar un Pack de Cotillón
void registrarPackCotillon(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string nombre_pack, elementos_str;
        string dni_hincha;
        vector<string> elementos;

        cout << "=== REGISTRO DE PACK DE COTILLÓN ===" << endl;
        
        // Validación de nombre de pack
        do {
            cout << "Nombre del Pack: ";
            getline(cin, nombre_pack);
            
            if (nombre_pack.length() < 3 || nombre_pack.length() > 100) {
                cout << "Nombre de pack inválido (3-100 caracteres)." << endl;
            }
        } while (nombre_pack.length() < 3 || nombre_pack.length() > 100);

        // Validación de elementos
        do {
            cout << "Elementos del Pack (separados por coma): ";
            getline(cin, elementos_str);
            
            // Separar y validar elementos
            stringstream ss(elementos_str);
            string elemento;
            elementos.clear();
            bool elementos_validos = true;
            
            while (getline(ss, elemento, ',')) {
                // Eliminar espacios
                elemento.erase(0, elemento.find_first_not_of(" "));
                elemento.erase(elemento.find_last_not_of(" ") + 1);
                
                if (elemento.length() < 2 || elemento.length() > 50) {
                    cout << "Elemento inválido: " << elemento << endl;
                    elementos_validos = false;
                    break;
                }
                elementos.push_back(elemento);
            }
            
            if (!elementos_validos) {
                cout << "Por favor, ingrese elementos válidos (2-50 caracteres)." << endl;
            }
        } while (elementos.empty());

        // Mostrar hinchas disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT hincha_id, nombre, dni FROM Hinchas;");
            
            cout << "\nHinchas disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " 
                     << row[1].as<string>() << " (DNI: " 
                     << row[2].as<string>() << ")" << endl;
            }
        }

        // Validación de hincha y límite de packs
        int hincha_id = 0;
        do {
            cout << "\nIngrese DNI del hincha para este pack: ";
            getline(cin, dni_hincha);

            // Obtener ID del hincha
            result R_hincha = W.exec_params("SELECT hincha_id FROM Hinchas WHERE dni = $1", dni_hincha);
            if (R_hincha.empty()) {
                cout << "Hincha no encontrado" << endl;
                continue;
            }
            
            hincha_id = R_hincha[0][0].as<int>();
            
            // Verificar límite de packs
            if (!verificarLimitePacks(c, hincha_id)) {
                cout << "El hincha ya ha alcanzado el límite de packs (3)." << endl;
                pausarSistema();
                return;
            }
        } while (hincha_id == 0);

        // Verificar si el hincha ya tiene un pack con este nombre
        {
            nontransaction N(c);
            result R = N.exec_params(
                "SELECT 1 FROM PacksCotillon pc "
                "JOIN PacksHinchas ph ON pc.pack_id = ph.pack_id "
                "WHERE ph.hincha_id = $1 AND pc.nombre = $2", 
                hincha_id, nombre_pack
            );
            
            if (!R.empty()) {
                cout << "El hincha ya tiene un pack con este nombre." << endl;
                pausarSistema();
                return;
            }
        }

        // Convertir elementos a formato PostgreSQL array
        string elementos_pg = "{" + elementos_str + "}";

        // Insertar Pack de Cotillón
        string query_pack = "INSERT INTO PacksCotillon (nombre, elementos) "
                            "VALUES ($1, $2) RETURNING pack_id;";
        
        result R_pack = W.exec_params(query_pack, nombre_pack, elementos_pg);
        int pack_id = R_pack[0][0].as<int>();

        // Asociar pack con hincha
        string query_pack_hincha = "INSERT INTO PacksHinchas (hincha_id, pack_id) "
                                   "VALUES ($1, $2);";
        W.exec_params(query_pack_hincha, hincha_id, pack_id);

        W.commit();

        cout << "\nPack de Cotillón registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del Pack de Cotillón: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para mostrar información de hinchas
void consultarHinchas(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec("SELECT h1.nombre, h1.dni, h1.nacionalidad, "
                          "h2.nombre AS invitado_por "
                          "FROM Hinchas h1 "
                          "LEFT JOIN Hinchas h2 ON h1.invitado_por = h2.hincha_id;");
        
        cout << "=== HINCHAS REGISTRADOS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nNombre: " << row[0].as<string>() << endl;
            cout << "DNI: " << row[1].as<string>() << endl;
            cout << "Nacionalidad: " << row[2].as<string>() << endl;
            cout << "Invitado por: " << (row[3].is_null() ? "Nadie" : row[3].as<string>()) << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar hinchas: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para consultar partidos
void consultarPartidos(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec("SELECT p.partido_id, "
                          "e1.pais AS equipo1, "
                          "e2.pais AS equipo2, "
                          "p.fecha, "
                          "est.nombre AS estadio, "
                          "p.espectadores, "
                          "array_agg(DISTINCT a.nombre) AS arbitros "
                          "FROM Partidos p "
                          "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
                          "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id "
                          "JOIN Estadios est ON p.estadio_id = est.estadio_id "
                          "LEFT JOIN ArbitrosPartidos ap ON p.partido_id = ap.partido_id "
                          "LEFT JOIN Arbitros a ON ap.arbitro_id = a.arbitro_id "
                          "GROUP BY p.partido_id, e1.pais, e2.pais, p.fecha, est.nombre;");
        
        cout << "=== PARTIDOS REGISTRADOS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nPartido #" << row[0].as<int>() << endl;
            cout << "Equipos: " << row[1].as<string>() << " vs " << row[2].as<string>() << endl;
            cout << "Fecha: " << row[3].as<string>() << endl;
            cout << "Estadio: " << row[4].as<string>() << endl;
            cout << "Espectadores: " << row[5].as<int>() << endl;
            
            // Imprimir árbitros
            string arbitros = row[6].as<string>();
            cout << "Árbitros: " << arbitros.substr(1, arbitros.length() - 2) << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar partidos: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para registrar consumo de cerveza
void registrarConsumoCerveza(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string dni_hincha;
        int partido_id, cantidad_cervezas;

        // Mostrar hinchas disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT hincha_id, nombre, dni FROM Hinchas;");
            
            cout << "Hinchas disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " 
                     << row[1].as<string>() << " (DNI: " 
                     << row[2].as<string>() << ")" << endl;
            }
        }

        cout << "\nIngrese DNI del hincha: ";
        getline(cin, dni_hincha);

        // Mostrar partidos disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT p.partido_id, e1.pais AS equipo1, e2.pais AS equipo2, p.fecha "
                              "FROM Partidos p "
                              "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
                              "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id;");
            
            cout << "\nPartidos disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " 
                     << row[1].as<string>() << " vs " 
                     << row[2].as<string>() << " (" 
                     << row[3].as<string>() << ")" << endl;
            }
        }

        cout << "\nSeleccione ID del partido: ";
        cin >> partido_id;
        limpiarEntrada();

        cout << "Cantidad de cervezas consumidas: ";
        cin >> cantidad_cervezas;
        limpiarEntrada();

        // Obtener ID del hincha
        result R_hincha = W.exec_params("SELECT hincha_id FROM Hinchas WHERE dni = $1", dni_hincha);
        if (R_hincha.empty()) {
            cout << "Hincha no encontrado" << endl;
            pausarSistema();
            return;
        }
        int hincha_id = R_hincha[0][0].as<int>();

        // Insertar consumo de cerveza
        string query = "INSERT INTO ConsumoCerveza (hincha_id, partido_id, cantidad_cervezas) "
                       "VALUES ($1, $2, $3) "
                       "ON CONFLICT (hincha_id, partido_id) DO UPDATE "
                       "SET cantidad_cervezas = $3;";
        
        W.exec_params(query, hincha_id, partido_id, cantidad_cervezas);
        W.commit();

        cout << "\nConsumo de cerveza registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del consumo de cerveza: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para añadir hospedaje de hincha
void registrarHospedajeHincha(connection &c) {
    limpiarTerminal();
    try {
        work W(c);
        
        string dni_hincha, hotel_id;

        // Mostrar hinchas disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT hincha_id, nombre, dni FROM Hinchas;");
            
            cout << "Hinchas disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << row[0].as<int>() << ". " 
                     << row[1].as<string>() << " (DNI: " 
                     << row[2].as<string>() << ")" << endl;
            }
        }

        cout << "\nIngrese DNI del hincha: ";
        getline(cin, dni_hincha);

        // Mostrar hoteles disponibles
        {
            nontransaction N(c);
            result R = N.exec("SELECT hotel_id, nombre, direccion FROM Hoteles;");
            
            cout << "\nHoteles disponibles:" << endl;
            for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
                cout << "ID: " << row[0].as<string>() 
                     << " | " << row[1].as<string>() 
                     << " (" << row[2].as<string>() << ")" << endl;
            }
        }

        cout << "\nIngrese ID del hotel: ";
        getline(cin, hotel_id);

        // Obtener ID del hincha
        result R_hincha = W.exec_params("SELECT hincha_id FROM Hinchas WHERE dni = $1", dni_hincha);
        if (R_hincha.empty()) {
            cout << "Hincha no encontrado" << endl;
            pausarSistema();
            return;
        }
        int hincha_id = R_hincha[0][0].as<int>();

        // Insertar hospedaje
        string query = "INSERT INTO Hospedaje (hincha_id, hotel_id) "
                       "VALUES ($1, $2) "
                       "ON CONFLICT (hincha_id, hotel_id) DO NOTHING;";
        
        W.exec_params(query, hincha_id, hotel_id);
        W.commit();

        cout << "\nHospedaje registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante el registro del hospedaje: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para consultar equipos y sus partidos jugados
void consultarEquiposYPartidos(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT e.pais, e.mundiales_ganados, "
            "COUNT(p.partido_id) AS partidos_jugados, "
            "SUM(CASE WHEN p.equipo1_id = e.equipo_id OR p.equipo2_id = e.equipo_id THEN 1 ELSE 0 END) AS total_partidos "
            "FROM Equipos e "
            "LEFT JOIN Partidos p ON (p.equipo1_id = e.equipo_id OR p.equipo2_id = e.equipo_id) "
            "GROUP BY e.pais, e.mundiales_ganados "
            "ORDER BY total_partidos DESC;"
        );
        
        cout << "=== EQUIPOS Y PARTIDOS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nPaís: " << row[0].as<string>() << endl;
            cout << "Mundiales ganados: " << row[1].as<int>() << endl;
            cout << "Partidos en este Mundial: " << row[2].as<int>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar equipos y partidos: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para consultar consumo de cerveza por partido
void consultarConsumoCerveza(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT p.partido_id, "
            "e1.pais AS equipo1, "
            "e2.pais AS equipo2, "
            "SUM(cc.cantidad_cervezas) AS total_cervezas, "
            "COUNT(DISTINCT h.hincha_id) AS total_hinchas "
            "FROM Partidos p "
            "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
            "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id "
            "LEFT JOIN ConsumoCerveza cc ON p.partido_id = cc.partido_id "
            "LEFT JOIN Hinchas h ON cc.hincha_id = h.hincha_id "
            "GROUP BY p.partido_id, e1.pais, e2.pais "
            "ORDER BY total_cervezas DESC;"
        );
        
        cout << "=== CONSUMO DE CERVEZA POR PARTIDO ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nPartido: " << row[1].as<string>() << " vs " << row[2].as<string>() << endl;
            cout << "Total de cervezas consumidas: " << row[3].as<int>() << endl;
            cout << "Número de hinchas que consumieron: " << row[4].as<int>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar consumo de cerveza: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para consultar Fun Fests
void consultarFunFests(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT ff.funfest_id, ff.ciudad, ff.duracion, "
            "e1.pais AS equipo1, e2.pais AS equipo2 "
            "FROM FunFests ff "
            "JOIN Partidos p ON ff.partido_id = p.partido_id "
            "JOIN Equipos e1 ON p.equipo1_id = e1.equipo_id "
            "JOIN Equipos e2 ON p.equipo2_id = e2.equipo_id;"
        );
        
        cout << "=== FUN FESTS REGISTRADOS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nCódigo Fun Fest: " << row[0].as<string>() << endl;
            cout << "Ciudad: " << row[1].as<string>() << endl;
            cout << "Duración: " << row[2].as<string>() << endl;
            cout << "Partido transmitido: " << row[3].as<string>() << " vs " << row[4].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar Fun Fests: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para consultar hoteles y hospedaje
void consultarHotelesHospedaje(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT h.hotel_id, h.nombre, h.direccion, h.estrellas, "
            "COUNT(DISTINCT ho.hincha_id) AS hinchas_hospedados "
            "FROM Hoteles h "
            "LEFT JOIN Hospedaje ho ON h.hotel_id = ho.hotel_id "
            "GROUP BY h.hotel_id, h.nombre, h.direccion, h.estrellas "
            "ORDER BY hinchas_hospedados DESC;"
        );
        
        cout << "=== HOTELES Y HOSPEDAJE ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nHotel: " << row[1].as<string>() << endl;
            cout << "ID: " << row[0].as<string>() << endl;
            cout << "Dirección: " << row[2].as<string>() << endl;
            cout << "Estrellas: " << row[3].as<int>() << endl;
            cout << "Hinchas hospedados: " << row[4].as<int>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al consultar hoteles y hospedaje: " << e.what() << endl;
    }
    pausarSistema();
}

// Función para generar informe de packs de cotillón
void generarInformePacks(connection &c) {
    limpiarTerminal();
    try {
        nontransaction N(c);
        
        result R = N.exec(
            "SELECT h.nombre AS hincha, h.nacionalidad, "
            "COUNT(DISTINCT ph.pack_id) AS packs_obtenidos, "
            "array_agg(DISTINCT pc.nombre) AS nombres_packs "
            "FROM Hinchas h "
            "LEFT JOIN PacksHinchas ph ON h.hincha_id = ph.hincha_id "
            "LEFT JOIN PacksCotillon pc ON ph.pack_id = pc.pack_id "
            "GROUP BY h.hincha_id, h.nombre, h.nacionalidad "
            "ORDER BY packs_obtenidos DESC;"
        );
        
        cout << "=== INFORME DE PACKS DE COTILLÓN ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\nHincha: " << row[0].as<string>() << endl;
            cout << "Nacionalidad: " << row[1].as<string>() << endl;
            cout << "Packs obtenidos: " << row[2].as<int>() << endl;
            
            if (!row[3].is_null()) {
                string packs = row[3].as<string>();
                cout << "Nombres de packs: " << packs.substr(1, packs.length() - 2) << endl;
            }
        }
    } catch (const exception &e) {
        cerr << "Error al generar informe de packs: " << e.what() << endl;
    }
    pausarSistema();
}

// Menú principal actualizado
void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarTerminal();
        cout << "+----------------------------------+" << endl;
        cout << "|    SISTEMA MUNDIAL BRASIL 2014   |" << endl;
        cout << "+----------------------------------+" << endl;
        cout << "| REGISTRO:                        |" << endl;
        cout << "| 1. Hincha                        |" << endl;
        cout << "| 2. Hotel                         |" << endl;
        cout << "| 3. Equipo                        |" << endl;
        cout << "| 4. Partido                       |" << endl;
        cout << "| 5. Fun Fest                      |" << endl;
        cout << "| 6. Pack Cotillón                 |" << endl;
        cout << "| 7. Hospedaje Hincha              |" << endl;
        cout << "| 8. Consumo de Cerveza            |" << endl;
        cout << "+----------------------------------+" << endl;
        cout << "| CONSULTAS:                       |" << endl;
        cout << "| 9. Hinchas                       |" << endl;
        cout << "|10. Partidos                      |" << endl;
        cout << "|11. Equipos y Partidos            |" << endl;
        cout << "|12. Consumo de Cerveza            |" << endl;
        cout << "|13. Fun Fests                     |" << endl;
        cout << "|14. Hoteles y Hospedaje           |" << endl;
        cout << "|15. Informe de Packs              |" << endl;
        cout << "|16. Partidos por Etapa            |" << endl;
        cout << "+----------------------------------+" << endl;
        cout << "| 0. Salir del Sistema             |" << endl;
        cout << "+----------------------------------+" << endl;
        cout << "\nSeleccione una opción: ";

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
                // Registros
                case 1: registrarHincha(c); break;
                case 2: registrarHotel(c); break;
                case 3: registrarEquipo(c); break;
                case 4: registrarPartido(c); break;
                case 5: registrarFunFest(c); break;
                case 6: registrarPackCotillon(c); break;
                case 7: registrarHospedajeHincha(c); break;
                case 8: registrarConsumoCerveza(c); break;

                // Consultas
                case 9: consultarHinchas(c); break;
                case 10: consultarPartidos(c); break;
                case 11: consultarEquiposYPartidos(c); break;
                case 12: consultarConsumoCerveza(c); break;
                case 13: consultarFunFests(c); break;
                case 14: consultarHotelesHospedaje(c); break;
                case 15: generarInformePacks(c); break;
                case 16: consultarPartidosPorEtapa(c); break;

                case 0:
                    limpiarTerminal();
                    cout << "Finalizando el sistema..." << endl;
                    break;
                default:
                    limpiarTerminal();
                    cout << "ERROR: Opción no válida. Por favor, intente nuevamente." << endl;
                    pausarSistema();
                    break;
            }
        } catch (const exception &e) {
            cerr << "\nERROR en la operación: " << e.what() << endl;
            pausarSistema();
        }
    } while (opcion != 0);
}

// Función main
int main() {
    limpiarTerminal();
    
    cout << "+-----------------------------------+" << endl;
    cout << "|    SISTEMA MUNDIAL BRASIL 2014    |" << endl;
    cout << "+-----------------------------------+" << endl;
    
    try {
        cout << "\nConectando con la base de datos..." << endl;
        connection c = iniciarConexionBaseDatos();
        
        cout << "\nVerificando estructura de datos..." << endl;
        configurarEstructuraBD(c);
        
        cout << "\nSistema inicializado correctamente" << endl;
        
        // Iniciar menú principal
        pausarSistema();
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << "\nError crítico: " << e.what() << endl;
        cout << "\nEl sistema se cerrará debido a un error." << endl;
        pausarSistema();
        return 1;
    }
    
    cout << "\nGracias por usar el Sistema Mundial Brasil 2014" << endl;
    return 0;
}