#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>

using namespace std;
using namespace pqxx;

// Funciones de utilidad para la interfaz
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

// Función para validar la clasificación de edad de la película
bool validarClasificacion(const string& clasificacion) {
    vector<string> clasificacionesPermitidas;
    clasificacionesPermitidas.push_back("Apta todo público");
    clasificacionesPermitidas.push_back("+9 años");
    clasificacionesPermitidas.push_back("+15 años");
    clasificacionesPermitidas.push_back("+18 años");
    
    return find(clasificacionesPermitidas.begin(), clasificacionesPermitidas.end(), 
                clasificacion) != clasificacionesPermitidas.end();
}

// Función para ejecutar consultas SQL en la base de datos
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

// Función auxiliar para dar formato al array de países
string formatearPaises(const string& paises) {
    string resultado = "{";
    string entrada = paises;
    size_t pos = 0;
    string token;
    string delimitador = ",";
    
    // Limpieza de espacios inicial y final
    while (!entrada.empty() && isspace(entrada.front())) entrada.erase(0, 1);
    while (!entrada.empty() && isspace(entrada.back())) entrada.pop_back();
    
    // Procesar cada país
    while ((pos = entrada.find(delimitador)) != string::npos) {
        token = entrada.substr(0, pos);
        while (!token.empty() && isspace(token.front())) token.erase(0, 1);
        while (!token.empty() && isspace(token.back())) token.pop_back();
        
        if (!token.empty()) {
            resultado += "\"" + token + "\",";
        }
        entrada.erase(0, pos + delimitador.length());
    }
    
    // Procesar el último país
    while (!entrada.empty() && isspace(entrada.front())) entrada.erase(0, 1);
    while (!entrada.empty() && isspace(entrada.back())) entrada.pop_back();
    
    if (!entrada.empty()) {
        resultado += "\"" + entrada + "\"";
    }
    
    resultado += "}";
    return resultado;
}

// Función para establecer conexión con la base de datos
connection iniciarConexion() {
    const string CONFIGURACION_DB = 
        "dbname=peliculas "
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

// Función para inicializar las tablas en la base de datos
void inicializarTablas(connection &c) {
    try {
        string query_movies = "CREATE TABLE IF NOT EXISTS Movies ("
                            "movie_id SERIAL PRIMARY KEY, "
                            "distribution_title VARCHAR(100), "
                            "original_title VARCHAR(100), "
                            "genre VARCHAR(50), "
                            "original_language VARCHAR(50), "
                            "spanish_subtitles BOOLEAN, "
                            "origin_countries TEXT[], "
                            "production_year INTEGER, "
                            "website_url VARCHAR(200), "
                            "duration INTERVAL, "
                            "age_rating VARCHAR(20), "
                            "release_date_santiago DATE, "
                            "synopsis TEXT"
                            ");";
        ejecutarSQL(c, query_movies);

        string query_directors = "CREATE TABLE IF NOT EXISTS Directors ("
                               "director_id SERIAL PRIMARY KEY, "
                               "name VARCHAR(100) UNIQUE, "
                               "nationality VARCHAR(50)"
                               ");";
        ejecutarSQL(c, query_directors);

        string query_movies_directors = "CREATE TABLE IF NOT EXISTS Movies_Directors ("
                                      "movie_id INTEGER REFERENCES Movies(movie_id), "
                                      "director_id INTEGER REFERENCES Directors(director_id), "
                                      "PRIMARY KEY (movie_id, director_id)"
                                      ");";
        ejecutarSQL(c, query_movies_directors);

        string query_actors = "CREATE TABLE IF NOT EXISTS Actors ("
                            "actor_id SERIAL PRIMARY KEY, "
                            "name VARCHAR(100) UNIQUE, "
                            "nationality VARCHAR(50)"
                            ");";
        ejecutarSQL(c, query_actors);

        string query_characters = "CREATE TABLE IF NOT EXISTS Characters ("
                                "character_id SERIAL PRIMARY KEY, "
                                "character_name VARCHAR(100), "
                                "actor_id INTEGER REFERENCES Actors(actor_id)"
                                ");";
        ejecutarSQL(c, query_characters);

        string query_movies_actors = "CREATE TABLE IF NOT EXISTS Movies_Actors ("
                                   "movie_id INTEGER REFERENCES Movies(movie_id), "
                                   "actor_id INTEGER REFERENCES Actors(actor_id), "
                                   "character_id INTEGER REFERENCES Characters(character_id), "
                                   "PRIMARY KEY (movie_id, actor_id)"
                                   ");";
        ejecutarSQL(c, query_movies_actors);

        string query_theaters = "CREATE TABLE IF NOT EXISTS Theaters ("
                              "theater_id SERIAL PRIMARY KEY, "
                              "name VARCHAR(100), "
                              "address VARCHAR(200), "
                              "phone VARCHAR(20)"
                              ");";
        ejecutarSQL(c, query_theaters);

        string query_rooms = "CREATE TABLE IF NOT EXISTS Rooms ("
                           "room_id SERIAL PRIMARY KEY, "
                           "name VARCHAR(100), "
                           "number INTEGER, "
                           "seat_capacity INTEGER, "
                           "theater_id INTEGER REFERENCES Theaters(theater_id)"
                           ");";
        ejecutarSQL(c, query_rooms);

        string query_shows = "CREATE TABLE IF NOT EXISTS Shows ("
                           "show_id SERIAL PRIMARY KEY, "
                           "weekday VARCHAR(20), "
                           "start_time TIME, "
                           "room_id INTEGER REFERENCES Rooms(room_id), "
                           "movie_id INTEGER REFERENCES Movies(movie_id)"
                           ");";
        ejecutarSQL(c, query_shows);

        string query_promos = "CREATE TABLE IF NOT EXISTS Promotions ("
                            "promo_id SERIAL PRIMARY KEY, "
                            "description TEXT, "
                            "discount NUMERIC"
                            ");";
        ejecutarSQL(c, query_promos);

        string query_shows_promos = "CREATE TABLE IF NOT EXISTS Shows_Promotions ("
                                  "show_id INTEGER REFERENCES Shows(show_id), "
                                  "promo_id INTEGER REFERENCES Promotions(promo_id), "
                                  "PRIMARY KEY (show_id, promo_id)"
                                  ");";
        ejecutarSQL(c, query_shows_promos);

        string query_reviews = "CREATE TABLE IF NOT EXISTS Reviews ("
                             "review_id SERIAL PRIMARY KEY, "
                             "movie_id INTEGER REFERENCES Movies(movie_id), "
                             "reviewer_name VARCHAR(100), "
                             "reviewer_age INTEGER, "
                             "review_date TIMESTAMP, "
                             "rating VARCHAR(20), "
                             "comment TEXT"
                             ");";
        ejecutarSQL(c, query_reviews);

        cout << "Base de datos inicializada correctamente" << endl;
        system("sleep 1");
        cout << "Iniciando sistema de gestión..." << endl;
        system("sleep 2");
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        esperarEntrada();
        throw;
        return;
    }
}

// Funciones para visualización de datos
void mostrarCatalogo(connection &c) {
    limpiarPantalla();
    try {
        string query = "SELECT m.movie_id, m.distribution_title, m.genre, m.original_language, "
                      "m.spanish_subtitles, m.origin_countries, m.production_year, m.age_rating, "
                      "m.release_date_santiago, m.synopsis, d.name AS director "
                      "FROM Movies m "
                      "LEFT JOIN Movies_Directors md ON m.movie_id = md.movie_id "
                      "LEFT JOIN Directors d ON md.director_id = d.director_id;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "No hay películas registradas en el catálogo." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== CATÁLOGO DE PELÍCULAS ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Película #" << row["movie_id"].as<int>() << " ---" << endl;
            cout << "Título: " << row["distribution_title"].as<string>() << endl;
            cout << "Categoría: " << row["genre"].as<string>() << endl;
            cout << "Idioma: " << row["original_language"].as<string>() << endl;
            cout << "Subtítulos: " << (row["spanish_subtitles"].as<bool>() ? "Disponible" : "No disponible") << endl;
            cout << "Países: " << row["origin_countries"].as<string>() << endl;
            cout << "Año: " << row["production_year"].as<int>() << endl;
            cout << "Clasificación: " << row["age_rating"].as<string>() << endl;
            cout << "Estreno: " << row["release_date_santiago"].as<string>() << endl;
            cout << "Director: " << row["director"].as<string>() << endl;
            cout << "Sinopsis: " << row["synopsis"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al cargar el catálogo: " << e.what() << endl;
        throw;
    }
    esperarEntrada();
}

void mostrarComplejos(connection &c) {
    limpiarPantalla();
    try {
        string query = "SELECT * FROM Theaters;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "No hay complejos de cine registrados." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== COMPLEJOS DE CINE ===" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "\n--- Complejo #" << row["theater_id"].as<int>() << " ---" << endl;
            cout << "Nombre: " << row["name"].as<string>() << endl;
            cout << "Ubicación: " << row["address"].as<string>() << endl;
            cout << "Contacto: " << row["phone"].as<string>() << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al cargar información de complejos: " << e.what() << endl;
        throw;
    }
    esperarEntrada();
}

void mostrarCartelera(connection &c) {
    limpiarPantalla();
    try {
        string query = "SELECT s.show_id, s.weekday, s.start_time, "
                      "r.name AS room_name, m.distribution_title "
                      "FROM Shows s "
                      "LEFT JOIN Rooms r ON s.room_id = r.room_id "
                      "LEFT JOIN Movies m ON s.movie_id = m.movie_id "
                      "ORDER BY s.weekday, s.start_time;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "No hay funciones programadas en cartelera." << endl;
            esperarEntrada();
            return;
        }

        cout << "=== CARTELERA ACTUAL ===" << endl;
        string dia_actual = "";
        
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            string dia = row["weekday"].as<string>();
            
            // Agrupar por día de la semana
            if (dia != dia_actual) {
                cout << "\n=== " << dia << " ===" << endl;
                dia_actual = dia;
            }
            
            cout << "\nFunción #" << row["show_id"].as<int>() << endl;
            cout << "Película: " << row["distribution_title"].as<string>() << endl;
            cout << "Hora: " << row["start_time"].as<string>() << endl;
            cout << "Sala: " << row["room_name"].as<string>() << endl;
            cout << "------------------------" << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al cargar la cartelera: " << e.what() << endl;
    }
    esperarEntrada();
}

// Funciones para gestión de películas
void registrarPelicula(connection &c) {
    limpiarPantalla();
    try {
        work W(c);
        string titulo_dist, titulo_orig, genero, idioma, paises, url, clasificacion, sinopsis;
        int ano;
        bool subtitulos;
        vector<string> directores;
        typedef pair<string, vector<string> > ActorInfo;
        vector<ActorInfo> actores_personajes;

        cout << "=== REGISTRO DE NUEVA PELÍCULA ===" << endl << endl;
        
        cout << "Información General:" << endl;
        cout << "----------------------" << endl;
        cout << "Título para distribución: ";
        getline(cin, titulo_dist);

        cout << "Título original: ";
        getline(cin, titulo_orig);

        cout << "Género cinematográfico: ";
        getline(cin, genero);

        cout << "Idioma de audio original: ";
        getline(cin, idioma);

        cout << "¿Disponible con subtítulos en español? (s/n): ";
        string resp;
        getline(cin, resp);
        subtitulos = (resp == "s" || resp == "S");

        cout << "Países de producción (separados por coma): ";
        getline(cin, paises);

        // Validación del año
        bool ano_valido = false;
        do {
            cout << "Año de producción: ";
            string ano_str;
            getline(cin, ano_str);
            try {
                ano = stoi(ano_str);
                if (ano >= 1895 && ano <= 2030) { // Validación básica de año
                    ano_valido = true;
                } else {
                    cout << "Por favor, ingrese un año válido (1895-2030)" << endl;
                }
            } catch (...) {
                cout << "Por favor, ingrese un número válido" << endl;
            }
        } while (!ano_valido);

        cout << "URL del sitio oficial: ";
        getline(cin, url);

        // Validación de duración
        int horas, minutos;
        bool duracion_valida = false;
        do {
            cout << "\nDuración del filme:" << endl;
            try {
                cout << "Horas: ";
                string horas_str;
                getline(cin, horas_str);
                horas = stoi(horas_str);

                cout << "Minutos: ";
                string minutos_str;
                getline(cin, minutos_str);
                minutos = stoi(minutos_str);

                if (horas >= 0 && minutos >= 0 && minutos < 60 && (horas > 0 || minutos > 0)) {
                    duracion_valida = true;
                } else {
                    cout << "Por favor, ingrese una duración válida" << endl;
                }
            } catch (...) {
                cout << "Por favor, ingrese números válidos" << endl;
            }
        } while (!duracion_valida);

        // Validación de clasificación
        do {
            cout << "\nClasificación de edad:" << endl;
            cout << "1. Apta todo público" << endl;
            cout << "2. +9 años" << endl;
            cout << "3. +15 años" << endl;
            cout << "4. +18 años" << endl;
            cout << "Seleccione una opción: ";
            getline(cin, clasificacion);
            
            switch(clasificacion[0]) {
                case '1': clasificacion = "Apta todo público"; break;
                case '2': clasificacion = "+9 años"; break;
                case '3': clasificacion = "+15 años"; break;
                case '4': clasificacion = "+18 años"; break;
                default: clasificacion = "";
            }
        } while (!validarClasificacion(clasificacion));

        cout << "\nSinopsis de la película:" << endl;
        getline(cin, sinopsis);

        // Insertar la película en la base de datos
        string query = "INSERT INTO Movies (distribution_title, original_title, "
                      "genre, original_language, spanish_subtitles, origin_countries, "
                      "production_year, website_url, duration, age_rating, release_date_santiago, synopsis) "
                      "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, CURRENT_DATE, $11) "
                      "RETURNING movie_id;";

        result R = W.exec_params(query,
                               titulo_dist,
                               titulo_orig,
                               genero,
                               idioma,
                               subtitulos,
                               formatearPaises(paises),
                               ano,
                               url,
                               to_string(horas) + " hours " + to_string(minutos) + " minutes",
                               clasificacion,
                               sinopsis);

        int id_pelicula = R[0][0].as<int>();

        // Registro de directores
        cout << "\n=== REGISTRO DE DIRECTORES ===" << endl;
        string continuar_director;
        do {
            cout << "\nNombre del director (o escriba 'fin' para terminar): ";
            string nombre_director;
            getline(cin, nombre_director);
            
            if (nombre_director == "fin") break;

            cout << "Nacionalidad del director: ";
            string nacionalidad_director;
            getline(cin, nacionalidad_director);

            // Verificar si el director ya existe
            query = "SELECT director_id FROM Directors WHERE name = $1;";
            result R_director = W.exec_params(query, nombre_director);

            int id_director;
            if (R_director.empty()) {
                // Crear nuevo director
                query = "INSERT INTO Directors (name, nationality) "
                        "VALUES ($1, $2) RETURNING director_id;";
                R_director = W.exec_params(query, nombre_director, nacionalidad_director);
                id_director = R_director[0][0].as<int>();
            } else {
                // Actualizar nacionalidad del director existente
                id_director = R_director[0][0].as<int>();
                query = "UPDATE Directors SET nationality = $1 WHERE director_id = $2;";
                W.exec_params(query, nacionalidad_director, id_director);
            }

            // Asociar director con la película
            query = "INSERT INTO Movies_Directors (movie_id, director_id) VALUES ($1, $2);";
            W.exec_params(query, id_pelicula, id_director);

            cout << "¿Desea agregar otro director? (s/n): ";
            getline(cin, continuar_director);
        } while (continuar_director == "s" || continuar_director == "S");

        // Registro de actores y personajes
        cout << "\n=== REGISTRO DE REPARTO ===" << endl;
        string continuar_actor;
        do {
            cout << "\nNombre del actor/actriz (o escriba 'fin' para terminar): ";
            string nombre_actor;
            getline(cin, nombre_actor);
            
            if (nombre_actor == "fin") break;

            cout << "Nacionalidad del actor/actriz: ";
            string nacionalidad_actor;
            getline(cin, nacionalidad_actor);

            // Verificar si el actor ya existe
            query = "SELECT actor_id FROM Actors WHERE name = $1;";
            result R_actor = W.exec_params(query, nombre_actor);

            int id_actor;
            if (R_actor.empty()) {
                // Crear nuevo actor
                query = "INSERT INTO Actors (name, nationality) "
                        "VALUES ($1, $2) RETURNING actor_id;";
                R_actor = W.exec_params(query, nombre_actor, nacionalidad_actor);
                id_actor = R_actor[0][0].as<int>();
            } else {
                // Actualizar nacionalidad del actor existente
                id_actor = R_actor[0][0].as<int>();
                query = "UPDATE Actors SET nationality = $1 WHERE actor_id = $2;";
                W.exec_params(query, nacionalidad_actor, id_actor);
            }

            // Registro de personajes para este actor
            string continuar_personaje;
            do {
                cout << "Nombre del personaje interpretado: ";
                string nombre_personaje;
                getline(cin, nombre_personaje);

                // Registrar personaje
                query = "INSERT INTO Characters (character_name, actor_id) "
                       "VALUES ($1, $2) RETURNING character_id;";
                
                result R_personaje = W.exec_params(query, nombre_personaje, id_actor);
                int id_personaje = R_personaje[0][0].as<int>();

                // Asociar con la película
                query = "INSERT INTO Movies_Actors (movie_id, actor_id, character_id) "
                       "VALUES ($1, $2, $3);";
                W.exec_params(query, id_pelicula, id_actor, id_personaje);

                cout << "¿Este actor interpreta otro personaje? (s/n): ";
                getline(cin, continuar_personaje);
            } while (continuar_personaje == "s" || continuar_personaje == "S");

            cout << "¿Desea registrar otro actor? (s/n): ";
            getline(cin, continuar_actor);
        } while (continuar_actor == "s" || continuar_actor == "S");

        W.commit();
        cout << "\nPelícula registrada exitosamente en el sistema" << endl;

    } catch (const exception &e) {
        cerr << "\nError durante el registro de la película: " << e.what() << endl;
    }
    esperarEntrada();
}

void eliminarPelicula(connection &c) {
    limpiarPantalla();
    try {
        cout << "=== ELIMINACIÓN DE PELÍCULA ===" << endl << endl;
        
        // Mostrar catálogo actual
        cout << "Películas disponibles en el sistema:" << endl;
        cout << "--------------------------------" << endl;
        {   
            nontransaction N(c);
            result R = N.exec("SELECT movie_id, distribution_title, production_year FROM Movies ORDER BY distribution_title;");
            
            if (R.empty()) {
                cout << "No hay películas registradas en el sistema." << endl;
                esperarEntrada();
                return;
            }
            
            result::const_iterator row;
            for (row = R.begin(); row != R.end(); ++row) {
                cout << "ID: " << row[0].as<int>() 
                    << " | " << row[1].as<string>() 
                    << " (" << row[2].as<int>() << ")" << endl;
            }
        }    

        cout << "\nIngrese el ID de la película a eliminar (0 para cancelar): ";
        int id_pelicula;
        while (!(cin >> id_pelicula)) {
            cout << "Por favor, ingrese un número válido: ";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        limpiarEntrada();
        
        if (id_pelicula == 0) {
            cout << "Operación cancelada por el usuario" << endl;
            esperarEntrada();
            return;
        }

        // Iniciar transacción y verificar película
        work W(c);
        
        result R_check = W.exec_params("SELECT distribution_title, production_year FROM Movies WHERE movie_id = $1", 
                                     id_pelicula);
        if (R_check.empty()) {
            cout << "\nError: No existe una película con el ID especificado." << endl;
            esperarEntrada();
            return;
        }

        string titulo = R_check[0]["distribution_title"].as<string>();
        int ano = R_check[0]["production_year"].as<int>();
        
        // Verificar dependencias
        result R = W.exec_params("SELECT COUNT(*) FROM Shows WHERE movie_id = $1;",
                               id_pelicula);
        int funciones = R[0][0].as<int>();
        
        R = W.exec_params("SELECT COUNT(*) FROM Reviews WHERE movie_id = $1;",
                         id_pelicula);
        int opiniones = R[0][0].as<int>();

        cout << "\nSe eliminará la siguiente película:" << endl;
        cout << "\"" << titulo << "\" (" << ano << ")" << endl;
        
        if (funciones > 0 || opiniones > 0) {
            cout << "\n¡ADVERTENCIA!" << endl;
            cout << "Esta película tiene los siguientes registros asociados:" << endl;
            cout << "- Funciones programadas: " << funciones << endl;
            cout << "- Reseñas de usuarios: " << opiniones << endl;
            cout << "Al eliminar la película se eliminarán todos estos registros." << endl;
        }
        
        cout << "\n¿Está seguro de que desea eliminar esta película? (s/n): ";
        char confirmacion;
        cin >> confirmacion;
        limpiarEntrada();
        
        if (confirmacion != 's' && confirmacion != 'S') {
            cout << "Operación cancelada por el usuario" << endl;
            esperarEntrada();
            return;
        }

        // Proceso de eliminación en cascada
        cout << "\nEliminando registros asociados..." << endl;
        
        // Eliminar promociones de funciones
        W.exec_params("DELETE FROM Shows_Promotions WHERE show_id IN "
                     "(SELECT show_id FROM Shows WHERE movie_id = $1);",
                     id_pelicula);
        
        // Eliminar funciones
        W.exec_params("DELETE FROM Shows WHERE movie_id = $1;",
                     id_pelicula);
        
        // Eliminar reseñas
        W.exec_params("DELETE FROM Reviews WHERE movie_id = $1;",
                     id_pelicula);
        
        // Eliminar relaciones con actores y personajes
        W.exec_params("DELETE FROM Movies_Actors WHERE movie_id = $1;",
                     id_pelicula);
        
        // Eliminar relaciones con directores
        W.exec_params("DELETE FROM Movies_Directors WHERE movie_id = $1;",
                     id_pelicula);
        
        // Finalmente, eliminar la película
        W.exec_params("DELETE FROM Movies WHERE movie_id = $1;",
                     id_pelicula);

        W.commit();
        cout << "\nLa película \"" << titulo << "\" ha sido eliminada exitosamente" << endl;
        system("sleep 1"); 

    } catch (const exception &e) {
        cerr << "\nError durante la eliminación de la película: " << e.what() << endl;
    }
    esperarEntrada();
}

// Menú principal del sistema
void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarPantalla();
        cout << "+--------------------------------+" << endl;
        cout << "|     SISTEMA GESTOR DE CINEMA   |" << endl;
        cout << "+--------------------------------+" << endl;
        cout << "| 1. Ver Catálogo de Películas   |" << endl;
        cout << "| 2. Ver Complejos de Cine       |" << endl;
        cout << "| 3. Ver Cartelera Actual        |" << endl;
        cout << "| 4. Registrar Nueva Película    |" << endl;
        cout << "| 5. Eliminar Película           |" << endl;
        cout << "| 0. Salir del Sistema           |" << endl;
        cout << "+--------------------------------+" << endl;
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
                case 1: 
                    mostrarCatalogo(c); 
                    break;
                case 2: 
                    mostrarComplejos(c); 
                    break;
                case 3: 
                    mostrarCartelera(c); 
                    break;
                case 4:
                    registrarPelicula(c);
                    break;
                case 5:
                    eliminarPelicula(c);
                    break;
                case 0:
                    limpiarPantalla();
                    cout << "Finalizando el sistema..." << endl;
                    system("sleep 1");
                    break;
                default:
                    limpiarPantalla();
                    cout << "ERROR: Opción no válida. Por favor, intente nuevamente." << endl;
                    esperarEntrada();
                    break;
            }
        } catch (const exception &e) {
            cerr << "\nERROR en la operación: " << e.what() << endl;
            esperarEntrada();
        }
    } while (opcion != 0);
}

// Función principal
int main() {
    limpiarPantalla();
    
    cout << "+--------------------------------+" << endl;
    cout << "|      INICIALIZANDO SISTEMA     |" << endl;
    cout << "+--------------------------------+" << endl;
    
    try {
        cout << "\nConectando con la base de datos..." << endl;
        connection c = iniciarConexion();
        system("sleep 1");
        
        cout << "\nVerificando estructura de datos..." << endl;
        inicializarTablas(c);
        system("sleep 1");
        
        cout << "\nSistema inicializado correctamente" << endl;
        system("sleep 1");
        
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << "\nError crítico: " << e.what() << endl;
        cout << "\nEl sistema se cerrará debido a un error." << endl;
        esperarEntrada();
        return 1;
    }
    
    cout << "\nGracias por usar el Sistema Gestor de Cinema" << endl;
    system("sleep 1");
    return 0;
}
