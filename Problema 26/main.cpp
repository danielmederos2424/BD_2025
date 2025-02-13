#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarConsola()
{
    system("clear");
}

void pausa()
{
    cout << "\nPresione cualquier tecla para continuar...";
    cin.get();
}

// Validaciones básicas
bool esTextoValido(const string &str, size_t minLen, size_t maxLen)
{
    return str.length() >= minLen && str.length() <= maxLen;
}

bool validarAnio(int anio)
{
    return anio >= -3000 && anio <= 2024;
}

// Obtener entrada numérica válida
template <typename T>
T obtenerNumeroValido(const string &mensaje, T min, T max)
{
    T numero;
    string entrada;
    bool valido = false;
    do
    {
        cout << mensaje;
        getline(cin, entrada);
        stringstream ss(entrada);
        if (ss >> numero && numero >= min && numero <= max)
        {
            valido = true;
        }
        else
        {
            cout << "⚠ Entrada inválida: Ingrese un número entre " << min << " y " << max << endl;
        }
    } while (!valido);
    return numero;
}

// Obtener texto válido
string obtenerTextoValido(const string &mensaje, size_t minLen, size_t maxLen)
{
    string texto;
    bool valido = false;
    do
    {
        cout << mensaje;
        getline(cin, texto);
        if (esTextoValido(texto, minLen, maxLen))
        {
            valido = true;
        }
        else
        {
            cout << "⚠ El texto debe tener entre " << minLen << " y "
                 << maxLen << " caracteres." << endl;
        }
    } while (!valido);
    return texto;
}

// Conexión a la base de datos
connection iniciarConexion()
{
    const string CONFIGURACION_DB =
        "dbname=centro_cultural "
        "user=postgres "
        "password=1234admin "
        "hostaddr=127.0.0.1 "
        "port=5432";
    try
    {
        connection c(CONFIGURACION_DB);
        if (!c.is_open())
        {
            throw runtime_error("No se pudo establecer la conexión con la base de datos");
        }
        return c;
    }
    catch (const exception &e)
    {
        throw runtime_error("Error al conectar: " + string(e.what()));
    }
}

// Inicialización de tablas
void inicializarTablas(connection &c)
{
    try
    {
        work W(c);

        // Tabla Epocas
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Epocas (
                epoca_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) UNIQUE NOT NULL,
                caracteristicas TEXT NOT NULL,
                anio_inicio INTEGER NOT NULL,
                anio_fin INTEGER NOT NULL,
                CONSTRAINT check_periodo CHECK (anio_fin > anio_inicio)
            )
        )");

        // Tabla Generos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Generos (
                genero_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) UNIQUE NOT NULL,
                caracteristicas TEXT NOT NULL,
                origenes TEXT NOT NULL,
                epoca_id INTEGER REFERENCES Epocas(epoca_id)
            )
        )");

        // Tabla Musicos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Musicos (
                musico_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) UNIQUE NOT NULL,
                fecha_nacimiento INTEGER,
                fecha_muerte INTEGER,
                historia TEXT NOT NULL,
                genero_id INTEGER REFERENCES Generos(genero_id),
                CONSTRAINT check_anios CHECK (fecha_muerte IS NULL OR fecha_muerte > fecha_nacimiento)
            )
        )");

        // Tabla Instrumentos
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Instrumentos (
                instrumento_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) UNIQUE NOT NULL,
                foto TEXT NOT NULL,
                lugar_creacion VARCHAR(100) NOT NULL,
                creador VARCHAR(100) NOT NULL,
                tipo VARCHAR(50) NOT NULL
            )
        )");

        // Tabla Generos_Instrumentos (relación muchos a muchos)
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Generos_Instrumentos (
                genero_id INTEGER REFERENCES Generos(genero_id),
                instrumento_id INTEGER REFERENCES Instrumentos(instrumento_id),
                PRIMARY KEY (genero_id, instrumento_id)
            )
        )");

        // Tabla Obras
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Obras (
                obra_id SERIAL PRIMARY KEY,
                nombre VARCHAR(200) UNIQUE NOT NULL,
                anio INTEGER NOT NULL,
                partitura TEXT NOT NULL,
                genero_id INTEGER REFERENCES Generos(genero_id)
            )
        )");

        // Tabla Musicos_Obras (relación muchos a muchos)
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Musicos_Obras (
                musico_id INTEGER REFERENCES Musicos(musico_id),
                obra_id INTEGER REFERENCES Obras(obra_id),
                PRIMARY KEY (musico_id, obra_id)
            )
        )");

        W.commit();
    }
    catch (const exception &e)
    {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

// Funciones de registro
void registrarEpoca(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
                 REGISTRO DE ÉPOCA MUSICAL
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
         << endl;

    try
    {
        string nombre = obtenerTextoValido("▸ Nombre de la Época: ", 2, 100);
        string caracteristicas = obtenerTextoValido("▸ Características: ", 10, 1000);
        int anio_inicio = obtenerNumeroValido<int>("▸ Año de inicio: ", -3000, 2024);
        int anio_fin = obtenerNumeroValido<int>("▸ Año de fin: ", anio_inicio, 2024);

        work W(c);
        W.exec_params(
            "INSERT INTO Epocas (nombre, caracteristicas, anio_inicio, anio_fin) "
            "VALUES ($1, $2, $3, $4)",
            nombre, caracteristicas, anio_inicio, anio_fin);
        W.commit();

        cout << "\n✓ Época registrada exitosamente\n";
    }
    catch (const exception &e)
    {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

void registrarGenero(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
                 REGISTRO DE GÉNERO MUSICAL
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)" << endl;

    try {
        result epocas;
        {
            nontransaction N(c);
            epocas = N.exec("SELECT epoca_id, nombre FROM Epocas ORDER BY epoca_id");
        } 

        if (epocas.empty()) {
            cout << "\n⚠ No hay épocas registradas. Registre una época primero.\n";
            pausa();
            return;
        }

        cout << "\nÉpocas disponibles:\n";
        for (const auto &epoca : epocas) {
            cout << "→ " << epoca["epoca_id"].as<int>() << ": " << epoca["nombre"].as<string>() << endl;
        }

        string nombre = obtenerTextoValido("\n▸ Nombre del Género: ", 2, 100);
        string caracteristicas = obtenerTextoValido("▸ Características: ", 10, 1000);
        string origenes = obtenerTextoValido("▸ Orígenes: ", 10, 1000);
        int epoca_id = obtenerNumeroValido<int>("▸ ID de la Época: ", 1, 999999);

        work W(c);
        W.exec_params(
            "INSERT INTO Generos (nombre, caracteristicas, origenes, epoca_id) "
            "VALUES ($1, $2, $3, $4)",
            nombre, caracteristicas, origenes, epoca_id
        );
        W.commit();

        cout << "\n✓ Género registrado exitosamente\n";
    } catch (const exception &e) {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

void registrarObra(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
                 REGISTRO DE OBRA MUSICAL
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)" << endl;

    try {
        result generos, musicos;
        {
            nontransaction N(c);
            generos = N.exec("SELECT genero_id, nombre FROM Generos ORDER BY genero_id");
            musicos = N.exec("SELECT musico_id, nombre FROM Musicos ORDER BY musico_id");
        }

        if (generos.empty()) {
            cout << "\n⚠ No hay géneros registrados. Registre un género primero.\n";
            pausa();
            return;
        }

        cout << "\nGéneros disponibles:\n";
        for (const auto &genero : generos) {
            cout << "→ " << genero["genero_id"].as<int>() << ": " << genero["nombre"].as<string>() << endl;
        }

        string nombre = obtenerTextoValido("\n▸ Nombre de la Obra: ", 2, 200);
        int anio = obtenerNumeroValido<int>("▸ Año de creación: ", -3000, 2024);
        string partitura = obtenerTextoValido("▸ Partitura (ubicación): ", 5, 500);
        int genero_id = obtenerNumeroValido<int>("▸ ID del Género: ", 1, 999999);

        if (musicos.empty()) {
            cout << "\n⚠ No hay músicos registrados. Registre músicos primero.\n";
            pausa();
            return;
        }

        cout << "\nMúsicos disponibles:\n";
        for (const auto &musico : musicos) {
            cout << "→ " << musico["musico_id"].as<int>() << ": " << musico["nombre"].as<string>() << endl;
        }

        work W(c);
        
        result R = W.exec_params(
            "INSERT INTO Obras (nombre, anio, partitura, genero_id) "
            "VALUES ($1, $2, $3, $4) RETURNING obra_id",
            nombre, anio, partitura, genero_id
        );
        
        int obra_id = R[0][0].as<int>();

        cout << "\nSeleccione los músicos autores (0 para terminar):\n";
        int musico_id;
        bool al_menos_un_musico = false;

        do {
            musico_id = obtenerNumeroValido<int>("▸ ID del Músico: ", 0, 999999);
            if (musico_id != 0) {
                try {
                    W.exec_params(
                        "INSERT INTO Musicos_Obras (musico_id, obra_id) VALUES ($1, $2)",
                        musico_id, obra_id
                    );
                    cout << "✓ Músico agregado\n";
                    al_menos_un_musico = true;
                } catch (const exception &e) {
                    cout << "✗ Error al agregar músico: " << e.what() << endl;
                }
            }
        } while (musico_id != 0);

        if (!al_menos_un_musico) {
            cout << "\n⚠ Debe agregar al menos un músico a la obra.\n";
            W.abort();
            pausa();
            return;
        }

        W.commit();
        cout << "\n✓ Obra registrada exitosamente\n";
    } catch (const exception &e) {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

// Funciones de consulta
void consultarEpocas(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
                    ÉPOCAS MUSICALES
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
         << endl;

    try
    {
        nontransaction N(c);
        result R = N.exec(
            "SELECT e.*, string_agg(g.nombre, ', ') as generos "
            "FROM Epocas e "
            "LEFT JOIN Generos g ON e.epoca_id = g.epoca_id "
            "GROUP BY e.epoca_id "
            "ORDER BY e.anio_inicio");

        if (R.empty())
        {
            cout << "\n⚠ No hay épocas registradas\n";
        }
        else
        {
            for (const auto &row : R)
            {
                cout << "\n╭─────────────────────────────────╮\n";
                cout << "│ " << row["nombre"].as<string>() << endl;
                cout << "├─────────────────────────────────┤\n";
                cout << "│ Período: " << row["anio_inicio"].as<int>() << " - "
                     << row["anio_fin"].as<int>() << endl;
                cout << "│ Características:\n│ " << row["caracteristicas"].as<string>() << endl;
                cout << "│ Géneros:\n│ " << (row["generos"].is_null() ? "Ninguno" : row["generos"].as<string>()) << endl;
                cout << "╰─────────────────────────────────╯\n";
            }
        }
    }
    catch (const exception &e)
    {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

void consultarObrasGenero(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
                OBRAS POR GÉNERO MUSICAL
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
         << endl;

    try
    {
        string genero = obtenerTextoValido("▸ Nombre del Género: ", 2, 100);

        nontransaction N(c);
        result R = N.exec_params(
            "SELECT o.nombre as obra, o.anio, "
            "string_agg(m.nombre, ', ') as compositores "
            "FROM Obras o "
            "JOIN Generos g ON o.genero_id = g.genero_id "
            "LEFT JOIN Musicos_Obras mo ON o.obra_id = mo.obra_id "
            "LEFT JOIN Musicos m ON mo.musico_id = m.musico_id "
            "WHERE LOWER(g.nombre) = LOWER($1) "
            "GROUP BY o.obra_id, o.nombre, o.anio "
            "ORDER BY o.anio",
            genero);

        if (R.empty())
        {
            cout << "\n⚠ No se encontraron obras para este género\n";
        }
        else
        {
            for (const auto &row : R)
            {
                cout << "\n╭─────────────────────────────────╮\n";
                cout << "│ " << row["obra"].as<string>() << endl;
                cout << "├─────────────────────────────────┤\n";
                cout << "│ Año: " << row["anio"].as<int>() << endl;
                cout << "│ Compositores:\n│ " << row["compositores"].as<string>() << endl;
                cout << "╰─────────────────────────────────╯\n";
            }
        }
    }
    catch (const exception &e)
    {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

void consultarInstrumentosGenero(connection &c)
{
    limpiarConsola();
    cout << R"(
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯
             INSTRUMENTOS POR GÉNERO MUSICAL
⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
         << endl;

    try
    {
        string genero = obtenerTextoValido("▸ Nombre del Género: ", 2, 100);

        nontransaction N(c);
        result R = N.exec_params(
            "SELECT i.nombre, i.tipo, i.creador, i.lugar_creacion "
            "FROM Instrumentos i "
            "JOIN Generos_Instrumentos gi ON i.instrumento_id = gi.instrumento_id "
            "JOIN Generos g ON gi.genero_id = g.genero_id "
            "WHERE LOWER(g.nombre) = LOWER($1) "
            "ORDER BY i.nombre",
            genero);

        if (R.empty())
        {
            cout << "\n⚠ No se encontraron instrumentos para este género\n";
        }
        else
        {
            for (const auto &row : R)
            {
                cout << "\n╭─────────────────────────────────╮\n";
                cout << "│ " << row["nombre"].as<string>() << endl;
                cout << "├─────────────────────────────────┤\n";
                cout << "│ Tipo: " << row["tipo"].as<string>() << endl;
                cout << "│ Creador: " << row["creador"].as<string>() << endl;
                cout << "│ Lugar de creación: " << row["lugar_creacion"].as<string>() << endl;
                cout << "╰─────────────────────────────────╯\n";
            }
        }
    }
    catch (const exception &e)
    {
        cout << "\n✗ Error: " << e.what() << endl;
    }
    pausa();
}

void menuPrincipal(connection &c)
{
    int opcion;
    do
    {
        limpiarConsola();
        cout << R"(
   ╭──────────────────────────────────────────────────────╮
   │           CENTRO CULTURAL - HISTORIA MUSICAL         │
   ╰──────────────────────────────────────────────────────╯

   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯ MENÚ PRINCIPAL ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯

    1. Épocas Musicales
       • Registrar nueva época
       • Consultar épocas y sus géneros

    2. Géneros Musicales
       • Registrar nuevo género
       • Consultar instrumentos por género

    3. Obras Musicales
       • Registrar nueva obra
       • Consultar obras por género

    0. Salir del sistema

   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
             << endl;

        opcion = obtenerNumeroValido<int>("   Seleccione una opción: ", 0, 3);

        try
        {
            switch (opcion)
            {
            case 1:
            {
                limpiarConsola();
                cout << R"(
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯ ÉPOCAS MUSICALES ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯

    1. Registrar nueva época
    2. Consultar épocas
    
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
                     << endl;
                int subopcion = obtenerNumeroValido<int>("   Seleccione: ", 1, 2);
                if (subopcion == 1)
                    registrarEpoca(c);
                else
                    consultarEpocas(c);
                break;
            }
            case 2:
            {
                limpiarConsola();
                cout << R"(
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯ GÉNEROS MUSICALES ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯

    1. Registrar nuevo género
    2. Consultar instrumentos por género
    
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
                     << endl;
                int subopcion = obtenerNumeroValido<int>("   Seleccione: ", 1, 2);
                if (subopcion == 1)
                    registrarGenero(c);
                else
                    consultarInstrumentosGenero(c);
                break;
            }
            case 3:
            {
                limpiarConsola();
                cout << R"(
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯ OBRAS MUSICALES ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯

    1. Registrar nueva obra
    2. Consultar obras por género
    
   ⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯)"
                     << endl;
                int subopcion = obtenerNumeroValido<int>("   Seleccione: ", 1, 2);
                if (subopcion == 1)
                    registrarObra(c);
                else
                    consultarObrasGenero(c);
                break;
            }
            case 0:
                limpiarConsola();
                cout << R"(
   ╭──────────────────────────────────────────────────────╮
   │          ¡Gracias por usar nuestro sistema!          │
   │              Historia Musical Cultural               │
   ╰──────────────────────────────────────────────────────╯
)" << endl;
                system("sleep 1");
                break;
            }
        }
        catch (const exception &e)
        {
            cerr << "\n╭──────────────────────────────────────────────────────╮";
            cerr << "\n│ ✗ ERROR: " << e.what() << endl;
            cerr << "╰──────────────────────────────────────────────────────╯\n";
            pausa();
        }
    } while (opcion != 0);
}

int main()
{
    limpiarConsola();
    cout << R"(
   ╭──────────────────────────────────────────────────────╮
   │              HISTORIA MUSICAL CULTURAL               │
   │                   Centro Cultural                    │
   ╰──────────────────────────────────────────────────────╯

                    Iniciando sistema...
)" << endl;

    try
    {
        cout << "\n   ▸ Estableciendo conexión con la base de datos...";
        connection c = iniciarConexion();
        cout << "\n   ▸ Verificando estructura de datos...";
        inicializarTablas(c);
        cout << "\n   ▸ Inicialización completada.\n";

        cout << R"(
   ╭──────────────────────────────────────────────────────╮
   │             Sistema iniciado con éxito               │
   ╰──────────────────────────────────────────────────────╯
)" << endl;

        system("sleep 1");
        pausa();
        menuPrincipal(c);
    }
    catch (const exception &e)
    {
        cerr << "\n   ╭──────────────────────────────────────────────────────╮";
        cerr << "\n   │ ✗ Error fatal: " << e.what() << endl;
        cerr << "   ╰──────────────────────────────────────────────────────╯\n";
        return 1;
    }
    return 0;
}