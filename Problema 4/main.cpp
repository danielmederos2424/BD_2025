#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarConsola() {
    system("clear");
}

void limpiarBuffer(){
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausa() {
    cout << "Pulse Enter para continuar...";
    cin.get();

}

// Función auxiliar para validar la calificación de la película
bool validarCalificacion(const string& calificacion) {
    vector<string> calificacionesValidas = {
        "Apta todo público", "+9 años", "+15 años", "+18 años"
    };
    return find(calificacionesValidas.begin(), calificacionesValidas.end(), 
                calificacion) != calificacionesValidas.end();
}

// Función para ejecutar una consulta SQL a la base de datos
bool ejecutarConsulta(connection &c, const string &query) {
    try {
        work W(c);
        W.exec(query);
        W.commit();
        return true;
    } catch (const exception &e) {
        cerr << "\033[1;31mError SQL: " << e.what() << "\033[0m" << endl;
        return false;
    }
}


// Función auxiliar para formatear países en formato array de PostgreSQL
string formatPaisesArray(const string& paises) {
    string result = "{";
    string input = paises;
    size_t pos = 0;
    string token;
    string delimiter = ",";
    
    // Eliminar espacios en blanco al inicio y final
    while (!input.empty() && isspace(input.front())) input.erase(0, 1);
    while (!input.empty() && isspace(input.back())) input.pop_back();
    
    // Procesar cada país
    while ((pos = input.find(delimiter)) != string::npos) {
        token = input.substr(0, pos);
        // Eliminar espacios en blanco alrededor del token
        while (!token.empty() && isspace(token.front())) token.erase(0, 1);
        while (!token.empty() && isspace(token.back())) token.pop_back();
        
        if (!token.empty()) {
            result += "\"" + token + "\",";
        }
        input.erase(0, pos + delimiter.length());
    }
    
    // Procesar el último país
    while (!input.empty() && isspace(input.front())) input.erase(0, 1);
    while (!input.empty() && isspace(input.back())) input.pop_back();
    
    if (!input.empty()) {
        result += "\"" + input + "\"";
    }
    
    result += "}";
    return result;
}

// Funciones principales
// Función para establecer la conexión con la base de datos
connection conectar() {
    const string DATOS_CONEXION = 
        "dbname=peliculas "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";

    try {
        connection c(DATOS_CONEXION);
        if (!c.is_open()) {
            throw runtime_error("No se pudo establecer la conexión con la base de datos");
        }
        cout << "Conexión exitosa a la base de datos: " << c.dbname() << endl;
        return c;
    } catch (const exception& e) {
        throw runtime_error("Error de conexión: " + string(e.what()));
    }
    pausa();
}

// Función para crear las tablas necesarias en caso de que no existan previamente
void crearTablas(connection &c) {
    try {
        string query_peliculas = "CREATE TABLE IF NOT EXISTS Peliculas ("
                                "id_pelicula SERIAL PRIMARY KEY, "
                                "titulo_distribucion VARCHAR(100), "
                                "titulo_original VARCHAR(100), "
                                "genero VARCHAR(50), "
                                "idioma_original VARCHAR(50), "
                                "subtitulos_espanol BOOLEAN, "
                                "paises_origen TEXT[], "
                                "ano_produccion INTEGER, "
                                "url_sitio_web VARCHAR(200), "
                                "duracion INTERVAL, "
                                "calificacion VARCHAR(20), "
                                "fecha_estreno_santiago DATE, "
                                "resumen TEXT"
                                ");";
        ejecutarConsulta(c, query_peliculas);

        string query_directores = "CREATE TABLE IF NOT EXISTS Directores ("
                                "id_director SERIAL PRIMARY KEY, "
                                "nombre VARCHAR(100) UNIQUE, "  
                                "nacionalidad VARCHAR(50)"
                                ");";
        ejecutarConsulta(c, query_directores);

        string query_peliculas_directores = "CREATE TABLE IF NOT EXISTS Peliculas_Directores ("
                                          "id_pelicula INTEGER REFERENCES Peliculas(id_pelicula), "
                                          "id_director INTEGER REFERENCES Directores(id_director), "
                                          "PRIMARY KEY (id_pelicula, id_director)"
                                          ");";
        ejecutarConsulta(c, query_peliculas_directores);

        string query_actores = "CREATE TABLE IF NOT EXISTS Actores ("
                             "id_actor SERIAL PRIMARY KEY, "
                             "nombre VARCHAR(100) UNIQUE, "
                             "nacionalidad VARCHAR(50)"
                             ");";
        ejecutarConsulta(c, query_actores);

        string query_personajes = "CREATE TABLE IF NOT EXISTS Personajes ("
                                 "id_personaje SERIAL PRIMARY KEY, "
                                 "nombre_personaje VARCHAR(100), "
                                 "id_actor INTEGER REFERENCES Actores(id_actor)"
                                 ");";
        ejecutarConsulta(c, query_personajes);

        string query_peliculas_actores = "CREATE TABLE IF NOT EXISTS Peliculas_Actores ("
                                        "id_pelicula INTEGER REFERENCES Peliculas(id_pelicula), "
                                        "id_actor INTEGER REFERENCES Actores(id_actor), "
                                        "id_personaje INTEGER REFERENCES Personajes(id_personaje), "
                                        "PRIMARY KEY (id_pelicula, id_actor)"
                                        ");";
        ejecutarConsulta(c, query_peliculas_actores);

        string query_cines = "CREATE TABLE IF NOT EXISTS Cines ("
                            "id_cine SERIAL PRIMARY KEY, "
                            "nombre VARCHAR(100), "
                            "direccion VARCHAR(200), "
                            "telefono VARCHAR(20)"
                            ");";
        ejecutarConsulta(c, query_cines);

        string query_salas = "CREATE TABLE IF NOT EXISTS Salas ("
                            "id_sala SERIAL PRIMARY KEY, "
                            "nombre VARCHAR(100), "
                            "numero INTEGER, "
                            "cantidad_butacas INTEGER, "
                            "id_cine INTEGER REFERENCES Cines(id_cine)"
                            ");";
        ejecutarConsulta(c, query_salas);

        string query_funciones = "CREATE TABLE IF NOT EXISTS Funciones ("
                                "id_funcion SERIAL PRIMARY KEY, "
                                "dia_semana VARCHAR(20), "
                                "hora_comienzo TIME, "
                                "id_sala INTEGER REFERENCES Salas(id_sala), "
                                "id_pelicula INTEGER REFERENCES Peliculas(id_pelicula)"
                                ");";
        ejecutarConsulta(c, query_funciones);

        string query_promociones = "CREATE TABLE IF NOT EXISTS Promociones ("
                                  "id_promocion SERIAL PRIMARY KEY, "
                                  "descripcion TEXT, "
                                  "descuento NUMERIC"
                                  ");";
        ejecutarConsulta(c, query_promociones);

        string query_funciones_promociones = "CREATE TABLE IF NOT EXISTS Funciones_Promociones ("
                                            "id_funcion INTEGER REFERENCES Funciones(id_funcion), "
                                            "id_promocion INTEGER REFERENCES Promociones(id_promocion), "
                                            "PRIMARY KEY (id_funcion, id_promocion)"
                                            ");";
        ejecutarConsulta(c, query_funciones_promociones);

        string query_opiniones = "CREATE TABLE IF NOT EXISTS Opiniones ("
                                "id_opinion SERIAL PRIMARY KEY, "
                                "id_pelicula INTEGER REFERENCES Peliculas(id_pelicula), "
                                "nombre_persona VARCHAR(100), "
                                "edad INTEGER, "
                                "fecha_registro TIMESTAMP, "
                                "calificacion VARCHAR(20), "
                                "comentario TEXT"
                                ");";
        ejecutarConsulta(c, query_opiniones);

        cout << "Tablas comprobadas exitosamente" << endl;
        system("sleep 1");
        cout << "Cargando menú principal..." << endl;
        system("sleep 2");
    } catch (const exception &e) {
        cerr << "Error al crear las tablas: " << e.what() << endl;
        pausa();
        throw;
        return;
    }
}

// Funciones para mostrar datos
void mostrarPeliculas(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT p.id_pelicula, p.titulo_distribucion, p.genero, p.idioma_original, p.subtitulos_espanol, "
                       "p.paises_origen, p.ano_produccion, p.calificacion, p.fecha_estreno_santiago, p.resumen, "
                       "d.nombre AS director "
                       "FROM Peliculas p "
                       "LEFT JOIN Peliculas_Directores pd ON p.id_pelicula = pd.id_pelicula "
                       "LEFT JOIN Directores d ON pd.id_director = d.id_director;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Peliculas está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Peliculas:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_pelicula"].as<int>() << endl;
            cout << "Título: " << row["titulo_distribucion"].as<string>() << endl;
            cout << "Género: " << row["genero"].as<string>() << endl;
            cout << "Idioma original: " << row["idioma_original"].as<string>() << endl;
            cout << "Subtítulos en español: " << (row["subtitulos_espanol"].as<bool>() ? "Sí" : "No") << endl;
            cout << "Países de origen: " << row["paises_origen"].as<string>() << endl;
            cout << "Año de producción: " << row["ano_produccion"].as<int>() << endl;
            cout << "Calificación: " << row["calificacion"].as<string>() << endl;
            cout << "Fecha de estreno en Santiago: " << row["fecha_estreno_santiago"].as<string>() << endl;
            cout << "Resumen: " << row["resumen"].as<string>() << endl;
            cout << "Director: " << row["director"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Peliculas: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarDirectores(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT * FROM Directores;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Directores está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Directores:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_director"].as<int>() << endl;
            cout << "Nombre: " << row["nombre"].as<string>() << endl;
            cout << "Nacionalidad: " << row["nacionalidad"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Directores: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarActores(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT * FROM Actores;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Actores está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Actores:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_actor"].as<int>() << endl;
            cout << "Nombre: " << row["nombre"].as<string>() << endl;
            cout << "Nacionalidad: " << row["nacionalidad"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Actores: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarPersonajes(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT pe.id_personaje, pe.nombre_personaje, a.nombre AS actor "
                       "FROM Personajes pe "
                       "LEFT JOIN Actores a ON pe.id_actor = a.id_actor;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Personajes está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Personajes:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_personaje"].as<int>() << endl;
            cout << "Nombre: " << row["nombre_personaje"].as<string>() << endl;
            cout << "Actor: " << row["actor"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Personajes: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarCines(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT * FROM Cines;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Cines está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Cines:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_cine"].as<int>() << endl;
            cout << "Nombre: " << row["nombre"].as<string>() << endl;
            cout << "Dirección: " << row["direccion"].as<string>() << endl;
            cout << "Teléfono: " << row["telefono"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Cines: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarSalas(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT sa.id_sala, sa.nombre, sa.numero, sa.cantidad_butacas, c.nombre AS cine "
                       "FROM Salas sa "
                       "LEFT JOIN Cines c ON sa.id_cine = c.id_cine;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Salas está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Salas:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_sala"].as<int>() << endl;
            cout << "Nombre: " << row["nombre"].as<string>() << endl;
            cout << "Número: " << row["numero"].as<int>() << endl;
            cout << "Cantidad de butacas: " << row["cantidad_butacas"].as<int>() << endl;
            cout << "Cine: " << row["cine"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Salas: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarFunciones(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT fu.id_funcion, fu.dia_semana, fu.hora_comienzo, sa.nombre AS sala, p.titulo_distribucion AS pelicula "
                       "FROM Funciones fu "
                       "LEFT JOIN Salas sa ON fu.id_sala = sa.id_sala "
                       "LEFT JOIN Peliculas p ON fu.id_pelicula = p.id_pelicula;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Funciones está vacía." << endl;
            // limpiarBuffer();  
            pausa();         
            return;
        }

        cout << "Datos de la tabla Funciones:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_funcion"].as<int>() << endl;
            cout << "Día de la semana: " << row["dia_semana"].as<string>() << endl;
            cout << "Hora de comienzo: " << row["hora_comienzo"].as<string>() << endl;
            cout << "Sala: " << row["sala"].as<string>() << endl;
            cout << "Película: " << row["pelicula"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Funciones: " << e.what() << endl;
    }
    //limpiarBuffer();  
    pausa();
}

void mostrarPromociones(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT * FROM Promociones;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Promociones está vacía." << endl;
            return;
        }

        cout << "Datos de la tabla Promociones:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_promocion"].as<int>() << endl;
            cout << "Descripción: " << row["descripcion"].as<string>() << endl;
            cout << "Descuento: " << row["descuento"].as<double>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Promociones: " << e.what() << endl;
        throw;
    }
    pausa();
}

void mostrarOpiniones(connection &c) {
    limpiarConsola();
    try {
        string query = "SELECT * FROM Opiniones;";
        nontransaction N(c);
        result R(N.exec(query));

        if (R.size() == 0) {
            cout << "La tabla Opiniones está vacía." << endl;
            // limpiarBuffer();  
            pausa();         
            return;
        }

        cout << "Datos de la tabla Opiniones:" << endl;
        for (result::const_iterator row = R.begin(); row != R.end(); ++row) {
            cout << "ID: " << row["id_opinion"].as<int>() << endl;
            cout << "ID Película: " << row["id_pelicula"].as<int>() << endl;
            cout << "Nombre de la persona: " << row["nombre_persona"].as<string>() << endl;
            cout << "Edad: " << row["edad"].as<int>() << endl;
            cout << "Fecha de registro: " << row["fecha_registro"].as<string>() << endl;
            cout << "Calificación: " << row["calificacion"].as<string>() << endl;
            cout << "Comentario: " << row["comentario"].as<string>() << endl;
            cout << endl;
        }
    } catch (const exception &e) {
        cerr << "Error al mostrar datos de la tabla Opiniones: " << e.what() << endl;
    }
    limpiarBuffer();  
    pausa();
}

// Funciones para agregar y eliminar películas
// Función para agregar una nueva película
void agregarPelicula(connection &c) {
   limpiarConsola();
   try {
       work W(c);
       string titulo_dist, titulo_orig, genero, idioma, paises, url, calificacion, resumen;
       int ano;
       bool subtitulos;
       vector<string> directores;
       vector<pair<string, vector<string>>> actores_personajes;

       cout << "=== AGREGAR NUEVA PELÍCULA ===" << endl;
       
       cout << "Título de distribución: ";
       getline(cin, titulo_dist);

       cout << "Título original: ";
       getline(cin, titulo_orig);

       cout << "Género: ";
       getline(cin, genero);

       cout << "Idioma original: ";
       getline(cin, idioma);

       cout << "¿Tiene subtítulos en español? (s/n): ";
       string resp;
       getline(cin, resp);
       subtitulos = (resp == "s" || resp == "S");

       cout << "Países de origen (separados por coma): ";
       getline(cin, paises);

       cout << "Año de producción: ";
       string ano_str;
       getline(cin, ano_str);
       ano = stoi(ano_str);

       cout << "URL del sitio web: ";
       getline(cin, url);

       // Validar y obtener duración
       int horas, minutos;
       do {
           cout << "Duración (horas): ";
           cin >> horas;
           cout << "Duración (minutos): ";
           cin >> minutos;
       } while (horas < 0 || minutos < 0 || minutos >= 60);
       cin.ignore();

       // Validar y obtener calificación
       do {
           cout << "Calificación (Apta todo público, +9 años, +15 años, +18 años): ";
           getline(cin, calificacion);
       } while (!validarCalificacion(calificacion));

       cout << "Resumen: ";
       getline(cin, resumen);

       // Insertar la película
       string query = "INSERT INTO Peliculas (titulo_distribucion, titulo_original, "
                     "genero, idioma_original, subtitulos_espanol, paises_origen, "
                     "ano_produccion, url_sitio_web, duracion, calificacion, fecha_estreno_santiago, resumen) "
                     "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, CURRENT_DATE, $11) RETURNING id_pelicula;";

       result R = W.exec_params(query,
                              titulo_dist,
                              titulo_orig,
                              genero,
                              idioma,
                              subtitulos,
                              formatPaisesArray(paises),
                              ano,
                              url,
                              to_string(horas) + " hours " + to_string(minutos) + " minutes",
                              calificacion,
                              resumen);

       int id_pelicula = R[0][0].as<int>();

       // Agregar directores
       string mas_directores;
       do {
           cout << "Nombre del director (o 'fin' para terminar): ";
           string nombre_director;
           getline(cin, nombre_director);
           if (nombre_director == "fin") break;

           cout << "Nacionalidad del director: ";
           string nacionalidad_director;
           getline(cin, nacionalidad_director);

           // Primero intentamos obtener el director existente
           query = "SELECT id_director FROM Directores WHERE nombre = $1;";
           result R_director = W.exec_params(query, nombre_director);

           int id_director;
           if (R_director.empty()) {
               // Si no existe, lo insertamos
               query = "INSERT INTO Directores (nombre, nacionalidad) "
                       "VALUES ($1, $2) RETURNING id_director;";
               R_director = W.exec_params(query, nombre_director, nacionalidad_director);
               id_director = R_director[0][0].as<int>();
           } else {
               // Si existe, actualizamos su nacionalidad
               id_director = R_director[0][0].as<int>();
               query = "UPDATE Directores SET nacionalidad = $1 WHERE id_director = $2;";
               W.exec_params(query, nacionalidad_director, id_director);
           }

           // Relacionamos el director con la película
           query = "INSERT INTO Peliculas_Directores (id_pelicula, id_director) VALUES ($1, $2);";
           W.exec_params(query, id_pelicula, id_director);

           cout << "¿Agregar otro director? (s/n): ";
           getline(cin, mas_directores);
       } while (mas_directores == "s" || mas_directores == "S");

       // Agregar actores y personajes
       string mas_actores;
       do {
           cout << "Nombre del actor (o 'fin' para terminar): ";
           string nombre_actor;
           getline(cin, nombre_actor);
           if (nombre_actor == "fin") break;

           cout << "Nacionalidad del actor: ";
           string nacionalidad_actor;
           getline(cin, nacionalidad_actor);

           // Primero intentamos obtener el actor existente
           query = "SELECT id_actor FROM Actores WHERE nombre = $1;";
           result R_actor = W.exec_params(query, nombre_actor);

           int id_actor;
           if (R_actor.empty()) {
               // Si no existe, lo insertamos
               query = "INSERT INTO Actores (nombre, nacionalidad) "
                       "VALUES ($1, $2) RETURNING id_actor;";
               R_actor = W.exec_params(query, nombre_actor, nacionalidad_actor);
               id_actor = R_actor[0][0].as<int>();
           } else {
               // Si existe, actualizamos su nacionalidad
               id_actor = R_actor[0][0].as<int>();
               query = "UPDATE Actores SET nacionalidad = $1 WHERE id_actor = $2;";
               W.exec_params(query, nacionalidad_actor, id_actor);
           }

           // Agregar personajes para este actor
           string mas_personajes;
           do {
               cout << "Nombre del personaje: ";
               string nombre_personaje;
               getline(cin, nombre_personaje);

               query = "INSERT INTO Personajes (nombre_personaje, id_actor) "
                      "VALUES ($1, $2) RETURNING id_personaje;";
               
               result R_personaje = W.exec_params(query, nombre_personaje, id_actor);
               int id_personaje = R_personaje[0][0].as<int>();

               // Relacionar con la película
               query = "INSERT INTO Peliculas_Actores (id_pelicula, id_actor, id_personaje) "
                      "VALUES ($1, $2, $3);";
               W.exec_params(query, id_pelicula, id_actor, id_personaje);

               cout << "¿Agregar otro personaje para este actor? (s/n): ";
               getline(cin, mas_personajes);
           } while (mas_personajes == "s" || mas_personajes == "S");

           cout << "¿Agregar otro actor? (s/n): ";
           getline(cin, mas_actores);
       } while (mas_actores == "s" || mas_actores == "S");

       W.commit();
       cout << "Película agregada exitosamente" << endl;

   } catch (const exception &e) {
       cerr << "Error al agregar la película: " << e.what() << endl;
   }
   pausa();
}

// Función para eliminar una película
void eliminarPelicula(connection &c) {
    limpiarConsola();
    try {
        // Mostrar películas disponibles
        cout << "Películas disponibles:" << endl;
        {   
            nontransaction N(c);
            result R = N.exec("SELECT id_pelicula, titulo_distribucion FROM Peliculas;");
            
            if (R.empty()) {
                cout << "No hay películas registradas en el sistema." << endl;
                limpiarBuffer();
                pausa();
                return;
            }
            
            for (const auto &row : R) {
                cout << row[0].as<int>() << ". " << row[1].as<string>() << endl;
            }
        }   

        cout << "\nIngrese el ID de la película a eliminar (0 para cancelar): ";
        int id_pelicula;
        while (!(cin >> id_pelicula)) {
            cout << "Por favor, ingrese un número válido: ";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
        limpiarBuffer();
        
        if (id_pelicula == 0) {
            cout << "Operación cancelada" << endl;
            pausa();
            return;
        }

        // Verificar dependencias y pedir confirmación
        work W(c);
        
        result R_check = W.exec_params("SELECT titulo_distribucion FROM Peliculas WHERE id_pelicula = $1", 
                                     id_pelicula);
        if (R_check.empty()) {
            cout << "No existe una película con el ID especificado." << endl;
            pausa();
            return;
        }

        string titulo = R_check[0][0].as<string>();
        
        // Verificar dependencias
        result R = W.exec_params("SELECT COUNT(*) FROM Funciones WHERE id_pelicula = $1;",
                               id_pelicula);
        int funciones_dependientes = R[0][0].as<int>();
        
        R = W.exec_params("SELECT COUNT(*) FROM Opiniones WHERE id_pelicula = $1;",
                         id_pelicula);
        int opiniones_dependientes = R[0][0].as<int>();

        cout << "\nSe eliminará la película: " << titulo << endl;
        
        if (funciones_dependientes > 0 || opiniones_dependientes > 0) {
            cout << "\n¡ADVERTENCIA!" << endl;
            cout << "La película tiene:" << endl;
            cout << "- " << funciones_dependientes << " funciones asociadas" << endl;
            cout << "- " << opiniones_dependientes << " opiniones registradas" << endl;
            cout << "Al eliminar la película se eliminarán todos estos registros." << endl;
        }
        
        cout << "\n¿Está seguro de que desea continuar? (s/n): ";
        char confirmacion;
        cin >> confirmacion;
        limpiarBuffer();
        
        if (confirmacion != 's' && confirmacion != 'S') {
            cout << "Operación cancelada" << endl;
            pausa();
            return;
        }

        // Proceso de eliminación en orden para mantener la integridad referencial
        cout << "\nEliminando registros asociados..." << endl;
        
        W.exec_params("DELETE FROM Funciones_Promociones WHERE id_funcion IN "
                     "(SELECT id_funcion FROM Funciones WHERE id_pelicula = $1);",
                     id_pelicula);
        
        W.exec_params("DELETE FROM Funciones WHERE id_pelicula = $1;",
                     id_pelicula);
        
        W.exec_params("DELETE FROM Opiniones WHERE id_pelicula = $1;",
                     id_pelicula);
        
        W.exec_params("DELETE FROM Peliculas_Actores WHERE id_pelicula = $1;",
                     id_pelicula);
        
        W.exec_params("DELETE FROM Peliculas_Directores WHERE id_pelicula = $1;",
                     id_pelicula);
        
        W.exec_params("DELETE FROM Peliculas WHERE id_pelicula = $1;",
                     id_pelicula);

        W.commit();
        cout << "\nPelícula '" << titulo << "' eliminada exitosamente" << endl;
        system("sleep 1"); 
        pausa();

    } catch (const exception &e) {
        cerr << "\nError al eliminar la película: " << e.what() << endl;
        limpiarBuffer();
        pausa();
    }
}

// Menú para mostrar datos
void menuPrincipal(connection &c){
    int opcion;
    do {
        limpiarConsola();
        cout << ">>>> GESTIÓN DE PELÍCULAS <<<<" << endl;
        cout << "1. Mostrar Películas" << endl;
        cout << "2. Mostrar Directores" << endl;
        cout << "3. Mostrar Actores" << endl;
        cout << "4. Mostrar Personajes" << endl;
        cout << "5. Mostrar Cines" << endl;
        cout << "6. Mostrar Salas" << endl;
        cout << "7. Mostrar Funciones" << endl;
        cout << "8. Mostrar Promociones" << endl;
        cout << "9. Mostrar Opiniones" << endl;
        cout << "10. Agregar Pelicula" << endl;
        cout << "11. Eliminar Pelicula" << endl;
        cout << "0. Salir del programa" << endl;
        cout << ">> ";

        cin >> opcion;
        limpiarBuffer();

        switch (opcion) {
            case 1: 
                mostrarPeliculas(c); 
                break;
            case 2: 
                mostrarDirectores(c); 
                break;
            case 3:
                mostrarActores(c); 
                break;
            case 4: 
                mostrarPersonajes(c); 
                break;
            case 5: 
                mostrarCines(c); 
                break;
            case 6: 
                mostrarSalas(c); 
                break;
            case 7: 
                mostrarFunciones(c); 
                break;
            case 8: 
                mostrarPromociones(c); 
                break;
            case 9: 
                mostrarOpiniones(c); 
                break;
            case 10:
                agregarPelicula(c);
                break;
            case 11:
                eliminarPelicula(c);
                break;
            case 0:
                limpiarConsola();
                cout << "Programa finalizado..." << endl;
                system("sleep 1");
                break;
            default:
                limpiarConsola();
                cout << "Opción no válida, vuelva a intentarlo: " << endl;
                pausa();
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                break;
        }
    } while (opcion != 0);
}

// Función principal
int main() {
    limpiarConsola();
    try {
        connection c = conectar();
        system("sleep 1");
        crearTablas(c); 
        system("sleep 1");
        menuPrincipal(c);
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}

