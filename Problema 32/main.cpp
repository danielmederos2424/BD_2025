#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>

using namespace std;
using namespace pqxx;

// Funciones de utilidad para la interfaz
void limpiarConsola() {
    system("clear");
}

void limpiarBuffer(){
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausa() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

bool ejecutarConsulta(connection &c, const string &query) {
    try {
        work W(c);
        W.exec(query);
        W.commit();
        return true;
    } catch (const exception &e) {
        cerr << "\033[1;31mError en la consulta: " << e.what() << "\033[0m" << endl;
        return false;
    }
}

connection conectar() {
    const string CONFIGURACION_DB = 
        "dbname=videoclub "
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
    pausa();
}

void comprobarTablas(connection &c) {
    try {
        // Tabla de Películas
        string query_movies = "CREATE TABLE IF NOT EXISTS Movies ("
                            "movie_id SERIAL PRIMARY KEY, "
                            "title VARCHAR(100), "
                            "nationality VARCHAR(50), "
                            "production_company VARCHAR(100), "
                            "release_year INTEGER, "
                            "CONSTRAINT chk_release_year CHECK (release_year BETWEEN 1900 AND 2030)"
                            ");";
        ejecutarConsulta(c, query_movies);

        // Tabla de Directores
        string query_directors = "CREATE TABLE IF NOT EXISTS Directors ("
                               "director_id SERIAL PRIMARY KEY, "
                               "name VARCHAR(100), "
                               "nationality VARCHAR(50)"
                               ");";
        ejecutarConsulta(c, query_directors);

        // Tabla de Películas-Directores
        string query_movies_directors = "CREATE TABLE IF NOT EXISTS Movies_Directors ("
                                      "movie_id INTEGER REFERENCES Movies(movie_id), "
                                      "director_id INTEGER REFERENCES Directors(director_id), "
                                      "PRIMARY KEY (movie_id, director_id)"
                                      ");";
        ejecutarConsulta(c, query_movies_directors);

        // Tabla de Actores
        string query_actors = "CREATE TABLE IF NOT EXISTS Actors ("
                            "actor_id SERIAL PRIMARY KEY, "
                            "name VARCHAR(100), "
                            "nationality VARCHAR(50), "
                            "gender VARCHAR(10)"
                            ");";
        ejecutarConsulta(c, query_actors);

        // Tabla de Películas-Actores con distinción de actor principal
        string query_movies_actors = "CREATE TABLE IF NOT EXISTS Movies_Actors ("
                                   "movie_id INTEGER REFERENCES Movies(movie_id), "
                                   "actor_id INTEGER REFERENCES Actors(actor_id), "
                                   "is_main_actor BOOLEAN DEFAULT FALSE, "
                                   "PRIMARY KEY (movie_id, actor_id)"
                                   ");";
        ejecutarConsulta(c, query_movies_actors);

        // Tabla de Ejemplares de Películas
        string query_movie_copies = "CREATE TABLE IF NOT EXISTS MovieCopies ("
                                  "copy_id SERIAL PRIMARY KEY, "
                                  "movie_id INTEGER REFERENCES Movies(movie_id), "
                                  "conservation_state VARCHAR(50)"
                                  ");";
        ejecutarConsulta(c, query_movie_copies);

        // Tabla de Socios con aval
        string query_members = "CREATE TABLE IF NOT EXISTS Members ("
                             "member_id SERIAL PRIMARY KEY, "
                             "dni VARCHAR(20) UNIQUE, "
                             "name VARCHAR(100), "
                             "address VARCHAR(200), "
                             "phone VARCHAR(20), "
                             "sponsor_id INTEGER REFERENCES Members(member_id)"
                             ");";
        ejecutarConsulta(c, query_members);

        // Tabla de Alquileres
        string query_rentals = "CREATE TABLE IF NOT EXISTS Rentals ("
                             "rental_id SERIAL PRIMARY KEY, "
                             "copy_id INTEGER REFERENCES MovieCopies(copy_id), "
                             "member_id INTEGER REFERENCES Members(member_id), "
                             "rental_start_date DATE, "
                             "rental_end_date DATE, "
                             "CONSTRAINT chk_rental_dates CHECK (rental_end_date >= rental_start_date)"
                             ");";
        ejecutarConsulta(c, query_rentals);

        nontransaction N(c);
        string query_check_trigger = "SELECT EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'limit_member_rentals')";
        result R(N.exec(query_check_trigger));
        bool trigger_existe = R[0][0].as<bool>();

        // Solo crear el trigger si no existe
        if (!trigger_existe) {
            // Constraint adicional para límite de 4 alquileres por socio
            string query_rental_constraint = "CREATE OR REPLACE FUNCTION check_member_rentals() "
                                           "RETURNS TRIGGER AS $$ "
                                           "BEGIN "
                                           "  IF (SELECT COUNT(*) FROM Rentals "
                                           "      WHERE member_id = NEW.member_id "
                                           "      AND rental_end_date IS NULL) >= 4 THEN "
                                           "    RAISE EXCEPTION 'El socio no puede alquilar más de 4 ejemplares'; "
                                           "  END IF; "
                                           "  RETURN NEW; "
                                           "END; "
                                           "$$ LANGUAGE plpgsql;";
            ejecutarConsulta(c, query_rental_constraint);

            // Trigger para aplicar la restricción
            string query_rental_trigger = "CREATE TRIGGER limit_member_rentals "
                                        "BEFORE INSERT ON Rentals "
                                        "FOR EACH ROW "
                                        "EXECUTE FUNCTION check_member_rentals();";
            ejecutarConsulta(c, query_rental_trigger);
        }

        cout << "Base de datos de Video Club inicializada correctamente" << endl;

    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

bool validarDNI(const string& dni) {
    // Verificar que el DNI tenga exactamente 11 caracteres
    if (dni.length() != 11) {
        return false;
    }
    
    // Verificar que todos los caracteres sean dígitos
    for (char c : dni) {
        if (!isdigit(c)) {
            return false;
        }
    }
    
    return true;
}

// Función para registrar película
void registrarPelicula(connection &c) {
    limpiarConsola();
    try {
        work W(c);
        string titulo, nacionalidad, productora;
        int ano;

        cout << "=== REGISTRO DE NUEVA PELÍCULA ===" << endl << endl;
        
        cout << "Título de la película: ";
        getline(cin, titulo);

        cout << "Nacionalidad: ";
        getline(cin, nacionalidad);

        cout << "Productora: ";
        getline(cin, productora);

        // Validación del año de producción
        bool ano_valido = false;
        do {
            cout << "Año de producción: ";
            string ano_str;
            getline(cin, ano_str);
            try {
                ano = stoi(ano_str);
                if (ano >= 1900 && ano <= 2030) {
                    ano_valido = true;
                } else {
                    cout << "Por favor, ingrese un año válido (1900-2030)" << endl;
                }
            } catch (...) {
                cout << "Por favor, ingrese un número válido" << endl;
            }
        } while (!ano_valido);

        // Registro del director
        string nombre_director, nacionalidad_director;
        cout << "\nInformación del Director:" << endl;
        cout << "Nombre del director: ";
        getline(cin, nombre_director);

        cout << "Nacionalidad del director: ";
        getline(cin, nacionalidad_director);

        // Insertar película
        string query = "INSERT INTO Movies (title, nationality, production_company, release_year) "
                       "VALUES ($1, $2, $3, $4) RETURNING movie_id;";
        result R_pelicula = W.exec_params(query, titulo, nacionalidad, productora, ano);
        int id_pelicula = R_pelicula[0][0].as<int>();

        // Insertar director
        query = "INSERT INTO Directors (name, nationality) "
                "VALUES ($1, $2) ON CONFLICT (name) DO UPDATE "
                "SET nationality = $2 RETURNING director_id;";
        result R_director = W.exec_params(query, nombre_director, nacionalidad_director);
        int id_director = R_director[0][0].as<int>();

        // Asociar película con director
        query = "INSERT INTO Movies_Directors (movie_id, director_id) "
                "VALUES ($1, $2);";
        W.exec_params(query, id_pelicula, id_director);

        // Registro de actores
        string continuar_actor;
        do {
            string nombre_actor, nacionalidad_actor, genero_actor;
            bool es_actor_principal;

            cout << "\nInformación del Actor:" << endl;
            cout << "Nombre del actor: ";
            getline(cin, nombre_actor);

            cout << "Nacionalidad del actor: ";
            getline(cin, nacionalidad_actor);

            cout << "Género (Masculino/Femenino): ";
            getline(cin, genero_actor);

            cout << "¿Es actor principal? (s/n): ";
            string resp_principal;
            getline(cin, resp_principal);
            es_actor_principal = (resp_principal == "s" || resp_principal == "S");

            // Insertar actor
            query = "INSERT INTO Actors (name, nationality, gender) "
                    "VALUES ($1, $2, $3) ON CONFLICT (name) DO UPDATE "
                    "SET nationality = $2, gender = $3 RETURNING actor_id;";
            result R_actor = W.exec_params(query, nombre_actor, nacionalidad_actor, genero_actor);
            int id_actor = R_actor[0][0].as<int>();

            // Asociar actor con película
            query = "INSERT INTO Movies_Actors (movie_id, actor_id, is_main_actor) "
                    "VALUES ($1, $2, $3);";
            W.exec_params(query, id_pelicula, id_actor, es_actor_principal);

            cout << "¿Desea registrar otro actor? (s/n): ";
            getline(cin, continuar_actor);
        } while (continuar_actor == "s" || continuar_actor == "S");

        // Registro de ejemplares
        string continuar_ejemplar;
        do {
            string estado_conservacion;
            cout << "\nInformación del Ejemplar:" << endl;
            cout << "Estado de conservación: ";
            getline(cin, estado_conservacion);

            query = "INSERT INTO MovieCopies (movie_id, conservation_state) "
                    "VALUES ($1, $2);";
            W.exec_params(query, id_pelicula, estado_conservacion);

            cout << "¿Desea registrar otro ejemplar? (s/n): ";
            getline(cin, continuar_ejemplar);
        } while (continuar_ejemplar == "s" || continuar_ejemplar == "S");

        W.commit();
        cout << "\nPelícula registrada exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "\nError durante el registro de la película: " << e.what() << endl;
    }
    pausa();
}

// Función para registrar socio
void registrarSocio(connection &c) {
    limpiarConsola();
    try {
        work W(c);
        string dni, nombre, direccion, telefono;
        int socio_avalador;

        cout << "=== REGISTRO DE NUEVO SOCIO ===" << endl << endl;
        
        // Validación de DNI
        bool dni_valido = false;
        do {
            cout << "DNI del socio: ";
            getline(cin, dni);
            if (validarDNI(dni)) {
                // Verificar que el DNI no esté ya registrado
                result R = W.exec_params("SELECT COUNT(*) FROM Members WHERE dni = $1", dni);
                if (R[0][0].as<int>() == 0) {
                    dni_valido = true;
                } else {
                    cout << "El DNI ya está registrado. Intente nuevamente." << endl;
                }
            } else {
                cout << "DNI inválido. Formato correcto: 8 dígitos + 1 letra" << endl;
            }
        } while (!dni_valido);

        cout << "Nombre completo: ";
        getline(cin, nombre);

        cout << "Dirección: ";
        getline(cin, direccion);

        cout << "Teléfono: ";
        getline(cin, telefono);

        // Selección de socio avalador
        bool socio_valido = false;
        do {
            cout << "\nDNI del socio avalador (0 para omitir): ";
            string dni_avalador;
            getline(cin, dni_avalador);

            if (dni_avalador == "0") {
                socio_avalador = 0;
                socio_valido = true;
            } else {
                // Verificar que el socio avalador exista
                result R = W.exec_params("SELECT member_id FROM Members WHERE dni = $1", dni_avalador);
                if (!R.empty()) {
                    socio_avalador = R[0][0].as<int>();
                    socio_valido = true;
                } else {
                    cout << "No se encontró un socio con ese DNI. Intente nuevamente." << endl;
                }
            }
        } while (!socio_valido);

        // Insertar socio
        string query = "INSERT INTO Members (dni, name, address, phone, sponsor_id) "
                       "VALUES ($1, $2, $3, $4, $5);";
        
        if (socio_avalador == 0) {
            W.exec_params(query, dni, nombre, direccion, telefono, nullptr);
        } else {
            W.exec_params(query, dni, nombre, direccion, telefono, socio_avalador);
        }

        W.commit();
        cout << "\nSocio registrado exitosamente" << endl;

    } catch (const exception &e) {
        cerr << "\nError durante el registro del socio: " << e.what() << endl;
    }
    pausa();
}

// Función para mostrar películas
void mostrarPeliculas(connection &c) {
    limpiarConsola();
    try {
        // Usar una sola transacción no transaccional para todas las consultas
        nontransaction N(c);
        string query = "SELECT m.movie_id, m.title, m.nationality, m.production_company, m.release_year, "
                       "d.name AS director_name "
                       "FROM Movies m "
                       "LEFT JOIN Movies_Directors md ON m.movie_id = md.movie_id "
                       "LEFT JOIN Directors d ON md.director_id = d.director_id;";
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No hay películas registradas en el sistema." << endl;
            pausa();
            return;
        }

        cout << "=== CATÁLOGO DE PELÍCULAS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Película #" << row["movie_id"].as<int>() << " ---" << endl;
            cout << "Título: " << row["title"].as<string>() << endl;
            cout << "Nacionalidad: " << row["nationality"].as<string>() << endl;
            cout << "Productora: " << row["production_company"].as<string>() << endl;
            cout << "Año: " << row["release_year"].as<int>() << endl;
            cout << "Director: " << (row["director_name"].is_null() ? "No registrado" : row["director_name"].as<string>()) << endl;
            
            // Mostrar actores usando la misma transacción no transaccional
            string query_actores = "SELECT a.name, ma.is_main_actor "
                                   "FROM Actors a "
                                   "JOIN Movies_Actors ma ON a.actor_id = ma.actor_id "
                                   "WHERE ma.movie_id = " + to_string(row["movie_id"].as<int>());
            result RA(N.exec(query_actores));

            cout << "Actores:" << endl;
            for (result::const_iterator actor = RA.begin(); actor != RA.end(); ++actor) {
                cout << "- " << actor["name"].as<string>() 
                     << (actor["is_main_actor"].as<bool>() ? " (Actor Principal)" : "") << endl;
            }

            // Mostrar número de ejemplares usando la misma transacción
            string query_ejemplares = "SELECT COUNT(*) FROM MovieCopies "
                                      "WHERE movie_id = " + to_string(row["movie_id"].as<int>());
            result RC(N.exec(query_ejemplares));
            cout << "Ejemplares disponibles: " << RC[0][0].as<int>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al cargar el catálogo: " << e.what() << endl;
    }
    pausa();
}

void consultarPeliculasPorActor(connection &c) {
    limpiarConsola();
    try {
        // Obtener lista de actores
        nontransaction N(c);
        string query_actores = "SELECT DISTINCT name FROM Actors ORDER BY name";
        result R_actores(N.exec(query_actores));

        if (R_actores.empty()) {
            cout << "No hay actores registrados en el sistema." << endl;
            pausa();
            return;
        }

        // Mostrar lista de actores
        cout << "=== ACTORES REGISTRADOS ===" << endl;
        vector<string> actores;
        int i = 1;
        for (result::const_iterator row = R_actores.begin(); row != R_actores.end(); ++row) {
            actores.push_back(row[0].as<string>());
            cout << i << ". " << row[0].as<string>() << endl;
            i++;
        }

        // Solicitar selección de actor
        int seleccion;
        do {
            cout << "\nSeleccione un actor (0 para cancelar): ";
            cin >> seleccion;
            limpiarBuffer();

            if (seleccion == 0) {
                cout << "Operación cancelada." << endl;
                pausa();
                return;
            }

            if (seleccion < 1 || seleccion > actores.size()) {
                cout << "Selección inválida. Intente nuevamente." << endl;
            }
        } while (seleccion < 1 || seleccion > actores.size());

        string nombre_actor = actores[seleccion - 1];

        // Consultar películas del actor
        string query = "SELECT m.title, m.release_year, ma.is_main_actor "
                       "FROM Movies m "
                       "JOIN Movies_Actors ma ON m.movie_id = ma.movie_id "
                       "JOIN Actors a ON ma.actor_id = a.actor_id "
                       "WHERE a.name = $1";
        
        result R(N.exec_params(query, nombre_actor));

        if (R.empty()) {
            cout << "No se encontraron películas para el actor: " << nombre_actor << endl;
            pausa();
            return;
        }

        cout << "\nPelículas con " << nombre_actor << ":" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "- " << row["title"].as<string>() 
                 << " (" << row["release_year"].as<int>() << ")"
                 << (row["is_main_actor"].as<bool>() ? " [Actor Principal]" : "") 
                 << endl;
        }
    } catch (const exception &e) {
        cerr << "Error en la consulta: " << e.what() << endl;
    }
    pausa();
}

void consultarEjemplaresPelicula(connection &c) {
    limpiarConsola();
    try {
        // Obtener lista de películas
        nontransaction N(c);
        string query_peliculas = "SELECT DISTINCT title FROM Movies ORDER BY title";
        result R_peliculas(N.exec(query_peliculas));

        if (R_peliculas.empty()) {
            cout << "No hay películas registradas en el sistema." << endl;
            pausa();
            return;
        }

        // Mostrar lista de películas
        cout << "=== PELÍCULAS REGISTRADAS ===" << endl;
        vector<string> peliculas;
        int i = 1;
        for (result::const_iterator row = R_peliculas.begin(); row != R_peliculas.end(); ++row) {
            peliculas.push_back(row[0].as<string>());
            cout << i << ". " << row[0].as<string>() << endl;
            i++;
        }

        // Solicitar selección de película
        int seleccion;
        do {
            cout << "\nSeleccione una película (0 para cancelar): ";
            cin >> seleccion;
            limpiarBuffer();

            if (seleccion == 0) {
                cout << "Operación cancelada." << endl;
                pausa();
                return;
            }

            if (seleccion < 1 || seleccion > peliculas.size()) {
                cout << "Selección inválida. Intente nuevamente." << endl;
            }
        } while (seleccion < 1 || seleccion > peliculas.size());

        string titulo_pelicula = peliculas[seleccion - 1];

        // Consultar ejemplares de la película
        string query = "SELECT m.title, COUNT(mc.copy_id) as num_ejemplares, "
                       "STRING_AGG(mc.conservation_state, ', ') as estados "
                       "FROM Movies m "
                       "LEFT JOIN MovieCopies mc ON m.movie_id = mc.movie_id "
                       "WHERE m.title = $1 "
                       "GROUP BY m.title";
        
        result R(N.exec_params(query, titulo_pelicula));

        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "Película: " << row["title"].as<string>() << endl;
            cout << "Número de ejemplares: " << row["num_ejemplares"].as<int>() << endl;
            cout << "Estados de conservación: " << row["estados"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error en la consulta: " << e.what() << endl;
    }
    pausa();
}

void consultarPeliculasAlquiladasPorSocio(connection &c) {
    limpiarConsola();
    try {
        // Obtener lista de socios
        nontransaction N(c);
        string query_socios = "SELECT DISTINCT name FROM Members ORDER BY name";
        result R_socios(N.exec(query_socios));

        if (R_socios.empty()) {
            cout << "No hay socios registrados en el sistema." << endl;
            pausa();
            return;
        }

        // Mostrar lista de socios
        cout << "=== SOCIOS REGISTRADOS ===" << endl;
        vector<string> socios;
        int i = 1;
        for (result::const_iterator row = R_socios.begin(); row != R_socios.end(); ++row) {
            socios.push_back(row[0].as<string>());
            cout << i << ". " << row[0].as<string>() << endl;
            i++;
        }

        // Solicitar selección de socio
        int seleccion;
        do {
            cout << "\nSeleccione un socio (0 para cancelar): ";
            cin >> seleccion;
            limpiarBuffer();

            if (seleccion == 0) {
                cout << "Operación cancelada." << endl;
                pausa();
                return;
            }

            if (seleccion < 1 || seleccion > socios.size()) {
                cout << "Selección inválida. Intente nuevamente." << endl;
            }
        } while (seleccion < 1 || seleccion > socios.size());

        string nombre_socio = socios[seleccion - 1];

        // Consultar películas alquiladas por el socio
        string query = "SELECT m.title, r.rental_start_date, r.rental_end_date "
                       "FROM Members mem "
                       "JOIN Rentals r ON mem.member_id = r.member_id "
                       "JOIN MovieCopies mc ON r.copy_id = mc.copy_id "
                       "JOIN Movies m ON mc.movie_id = m.movie_id "
                       "WHERE mem.name = $1 "
                       "AND r.rental_end_date IS NULL";
        
        result R(N.exec_params(query, nombre_socio));

        if (R.empty()) {
            cout << "El socio " << nombre_socio << " no tiene películas alquiladas actualmente." << endl;
            pausa();
            return;
        }

        cout << "Películas alquiladas por " << nombre_socio << ":" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "- " << row["title"].as<string>() 
                 << " (Alquilada desde: " << row["rental_start_date"].as<string>() << ")" 
                 << endl;
        }
    } catch (const exception &e) {
        cerr << "Error en la consulta: " << e.what() << endl;
    }
    pausa();
}

void consultarSociosPresentados(connection &c) {
    limpiarConsola();
    try {
        // Obtener lista de socios que han presentado a otros
        nontransaction N(c);
        string query_avaladores = "SELECT DISTINCT name FROM Members "
                                  "WHERE member_id IN (SELECT DISTINCT sponsor_id FROM Members WHERE sponsor_id IS NOT NULL) "
                                  "ORDER BY name";
        result R_avaladores(N.exec(query_avaladores));

        if (R_avaladores.empty()) {
            cout << "No hay socios que hayan presentado a otros." << endl;
            pausa();
            return;
        }

        // Mostrar lista de socios avaladores
        cout << "=== SOCIOS QUE HAN PRESENTADO A OTROS ===" << endl;
        vector<string> avaladores;
        int i = 1;
        for (result::const_iterator row = R_avaladores.begin(); row != R_avaladores.end(); ++row) {
            avaladores.push_back(row[0].as<string>());
            cout << i << ". " << row[0].as<string>() << endl;
            i++;
        }

        // Solicitar selección de socio avalador
        int seleccion;
        do {
            cout << "\nSeleccione un socio avalador (0 para cancelar): ";
            cin >> seleccion;
            limpiarBuffer();

            if (seleccion == 0) {
                cout << "Operación cancelada." << endl;
                pausa();
                return;
            }

            if (seleccion < 1 || seleccion > avaladores.size()) {
                cout << "Selección inválida. Intente nuevamente." << endl;
            }
        } while (seleccion < 1 || seleccion > avaladores.size());

        string nombre_socio_avalador = avaladores[seleccion - 1];

        // Consultar socios presentados
        string query = "SELECT m.name, m.dni "
                       "FROM Members m "
                       "JOIN Members sponsor ON m.sponsor_id = sponsor.member_id "
                       "WHERE sponsor.name = $1";
        
        result R(N.exec_params(query, nombre_socio_avalador));

        if (R.empty()) {
            cout << "El socio " << nombre_socio_avalador << " no ha presentado a ningún socio." << endl;
            pausa();
            return;
        }

        cout << "Socios presentados por " << nombre_socio_avalador << ":" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "- " << row["name"].as<string>() 
                 << " (DNI: " << row["dni"].as<string>() << ")" 
                 << endl;
        }
    } catch (const exception &e) {
        cerr << "Error en la consulta: " << e.what() << endl;
    }
    pausa();
}

void consultarDirectoresPorNacionalidad(connection &c) {
    limpiarConsola();
    try {
        // Primero, obtener todas las nacionalidades distintas de directores
        nontransaction N(c);
        string query = "SELECT DISTINCT nationality "
                       "FROM Directors "
                       "WHERE nationality IS NOT NULL AND trim(nationality) != '' "
                       "ORDER BY nationality";
        
        result R(N.exec(query));

        if (R.empty()) {
            cout << "No se encontraron nacionalidades de directores." << endl;
            pausa();
            return;
        }

        // Mostrar nacionalidades con números
        cout << "=== NACIONALIDADES DE DIRECTORES ===" << endl;
        vector<string> nacionalidades;
        int i = 1;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            nacionalidades.push_back(row[0].as<string>());
            cout << i << ". " << row[0].as<string>() << endl;
            i++;
        }

        // Solicitar selección de nacionalidad
        int seleccion;
        cout << "\nSeleccione una nacionalidad (0 para cancelar): ";
        cin >> seleccion;
        limpiarBuffer();

        if (seleccion == 0 || seleccion > nacionalidades.size()) {
            cout << "Operación cancelada." << endl;
            pausa();
            return;
        }

        // Consultar directores de la nacionalidad seleccionada
        string nacionalidad_elegida = nacionalidades[seleccion - 1];
        string query_directores = "SELECT DISTINCT d.name "
                                  "FROM Directors d "
                                  "JOIN Movies_Directors md ON d.director_id = md.director_id "
                                  "WHERE LOWER(d.nationality) = LOWER($1)";
        
        result RD(N.exec_params(query_directores, nacionalidad_elegida));

        if (RD.empty()) {
            cout << "No se encontraron directores de " << nacionalidad_elegida << " en el sistema." << endl;
            pausa();
            return;
        }

        cout << "\nDirectores de " << nacionalidad_elegida << ":" << endl;
        for (result::const_iterator row = RD.begin(); row != RD.end(); ++row) {
            cout << "- " << row[0].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error en la consulta: " << e.what() << endl;
    }
    pausa();
}

void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarConsola();
        cout << "+--------------------------------+" << endl;
        cout << "|     SISTEMA GESTOR DE VIDEO    |" << endl;
        cout << "+--------------------------------+" << endl;
        cout << "| 1. Ver Catálogo de Películas   |" << endl;
        cout << "| 2. Registrar Nueva Película    |" << endl;
        cout << "| 3. Registrar Nuevo Socio       |" << endl;
        cout << "| 4. Consultas                   |" << endl;
        cout << "| 0. Salir del Sistema           |" << endl;
        cout << "+--------------------------------+" << endl;
        cout << "\nSeleccione una opción: ";

        if (!(cin >> opcion)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            opcion = -1;
        } else {
            limpiarBuffer();
        }

        switch (opcion) {
            case 1: 
                mostrarPeliculas(c); 
                break;
            case 2:
                registrarPelicula(c);
                break;
            case 3:
                registrarSocio(c);
                break;
            case 4: {
                int subopcion;
                do {
                    limpiarConsola();
                    cout << "+--------------------------------+" << endl;
                    cout << "|          MENÚ CONSULTAS        |" << endl;
                    cout << "+--------------------------------+" << endl;
                    cout << "| 1. Películas por Actor         |" << endl;
                    cout << "| 2. Ejemplares de Película      |" << endl;
                    cout << "| 3. Películas de un Socio       |" << endl;
                    cout << "| 4. Socios Presentados          |" << endl;
                    cout << "| 5. Directores por Nacionalidad |" << endl;
                    cout << "| 0. Volver                      |" << endl;
                    cout << "+--------------------------------+" << endl;
                    cout << "\nSeleccione una opción: ";

                    if (!(cin >> subopcion)) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        subopcion = -1;
                    } else {
                        limpiarBuffer();
                    }

                    switch (subopcion) {
                        case 1: consultarPeliculasPorActor(c); break;
                        case 2: consultarEjemplaresPelicula(c); break;
                        case 3: consultarPeliculasAlquiladasPorSocio(c); break;
                        case 4: consultarSociosPresentados(c); break;
                        case 5: consultarDirectoresPorNacionalidad(c); break;
                        case 0: break;
                        default:
                            cout << "Opción inválida" << endl;
                            pausa();
                            break;
                    }
                } while (subopcion != 0);
                break;
            }
            case 0:
                limpiarConsola();
                cout << "Finalizando el sistema..." << endl;
                system("sleep 1");
                break;
            default:
                limpiarConsola();
                cout << "ERROR: Opción no válida. Por favor, intente nuevamente." << endl;
                pausa();
                break;
        }
    } while (opcion != 0);
}

// Función principal
int main() {
    limpiarConsola();
    
    cout << "+--------------------------------+" << endl;
    cout << "|      INICIALIZANDO SISTEMA     |" << endl;
    cout << "+--------------------------------+" << endl;
    
    try {
        cout << "\nConectando con la base de datos..." << endl;
        connection c = conectar();
        system("sleep 1");
        
        cout << "\nVerificando estructura de datos..." << endl;
        comprobarTablas(c);
        system("sleep 1");
        
        cout << "\nSistema inicializado correctamente" << endl;
        system("sleep 1");
        
        pausa();
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << "\nError crítico: " << e.what() << endl;
        cout << "\nEl sistema se cerrará debido a un error." << endl;
        pausa();
        return 1;
    }

    return 0;
}
