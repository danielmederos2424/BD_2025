#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarConsola() {
    system("clear");
}

void pausa() {
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

// Validaciones
bool esNumeroTelefono(const string& str) {
    return !str.empty() && all_of(str.begin(), str.end(), ::isdigit);
}

bool esTextoValido(const string& str, size_t minLen, size_t maxLen) {
    return str.length() >= minLen && str.length() <= maxLen;
}

// Función para obtener un número válido dentro de un rango
template <typename T>
T obtenerNumeroValido(const string& mensaje, T min, T max) {
    T numero;
    string entrada;
    bool valido = false;
    do {
        cout << mensaje;
        getline(cin, entrada);
        stringstream ss(entrada);
        if (ss >> numero && numero >= min && numero <= max) {
            valido = true;
        } else {
            cout << "Error: Ingrese un número entre " << min << " y " << max << endl;
        }
    } while (!valido);
    return numero;
}

// Función para obtener texto válido dentro de un rango de longitud
string obtenerTextoValido(const string& mensaje, size_t minLen, size_t maxLen) {
    string texto;
    bool valido = false;
    do {
        cout << mensaje;
        getline(cin, texto);
        if (esTextoValido(texto, minLen, maxLen)) {
            valido = true;
        } else {
            cout << "Error: El texto debe tener entre " << minLen << " y " 
                 << maxLen << " caracteres." << endl;
        }
    } while (!valido);
    return texto;
}

bool validarFecha(const string& fecha) {
    if (fecha.length() != 10) return false;
    if (fecha[4] != '-' || fecha[7] != '-') return false;
    try {
        int year = stoi(fecha.substr(0, 4));
        int month = stoi(fecha.substr(5, 2));
        int day = stoi(fecha.substr(8, 2));
        if (year < 2024 || year > 2100) return false;
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;
        if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) return false;
        if (month == 2) {
            bool bisiesto = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            if (day > (bisiesto ? 29 : 28)) return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

// Conexión a la base de datos
connection iniciarConexion() {
    const string CONFIGURACION_DB =
        "dbname=muebleria "
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
    }
}

// Inicialización de tablas
void inicializarTablas(connection &c) {
    try {
        work W(c);

        // Tabla Maderas
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Maderas (
                madera_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) UNIQUE NOT NULL,
                dureza VARCHAR(50) NOT NULL
            )
        )");

        // Tabla ProveedoresMadera
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS ProveedoresMadera (
                proveedor_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) NOT NULL,
                telefono VARCHAR(20) NOT NULL CHECK (telefono ~ '^[0-9]+$')
            )
        )");

        // Tabla Maderas_Proveedores
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Maderas_Proveedores (
                madera_id INTEGER REFERENCES Maderas(madera_id),
                proveedor_id INTEGER REFERENCES ProveedoresMadera(proveedor_id),
                PRIMARY KEY (madera_id, proveedor_id)
            )
        )");

        // Tabla Muebles
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Muebles (
                mueble_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) NOT NULL,
                precio NUMERIC(10, 2) NOT NULL CHECK (precio > 0),
                tiene_promo BOOLEAN NOT NULL DEFAULT FALSE,
                alto NUMERIC(5, 2) NOT NULL,
                ancho NUMERIC(5, 2) NOT NULL,
                profundidad NUMERIC(5, 2) NOT NULL
            )
        )");

        // Tabla Muebles_Maderas
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Muebles_Maderas (
                mueble_id INTEGER REFERENCES Muebles(mueble_id),
                madera_id INTEGER REFERENCES Maderas(madera_id),
                PRIMARY KEY (mueble_id, madera_id)
            )
        )");

        // Tabla CombinacionesMuebles
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS CombinacionesMuebles (
                combinacion_id SERIAL PRIMARY KEY,
                mueble_principal_id INTEGER REFERENCES Muebles(mueble_id),
                mueble_secundario_id INTEGER REFERENCES Muebles(mueble_id),
                cantidad_sugerida INTEGER NOT NULL CHECK (cantidad_sugerida > 0)
            )
        )");

        // Tabla Clientes
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS Clientes (
                cliente_id SERIAL PRIMARY KEY,
                nombre VARCHAR(100) NOT NULL,
                telefono VARCHAR(20) NOT NULL CHECK (telefono ~ '^[0-9]+$')
            )
        )");

        // Tabla OrdenesCompra
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS OrdenesCompra (
                orden_id SERIAL PRIMARY KEY,
                cliente_id INTEGER REFERENCES Clientes(cliente_id),
                fecha DATE NOT NULL DEFAULT CURRENT_DATE,
                fecha_entrega DATE NOT NULL,
                calle VARCHAR(100) NOT NULL,
                localidad VARCHAR(100) NOT NULL,
                provincia VARCHAR(100) NOT NULL
            )
        )");

        // Tabla DetalleOrden
        W.exec(R"(
            CREATE TABLE IF NOT EXISTS DetalleOrden (
                orden_id INTEGER REFERENCES OrdenesCompra(orden_id),
                mueble_id INTEGER REFERENCES Muebles(mueble_id),
                cantidad INTEGER NOT NULL CHECK (cantidad > 0),
                PRIMARY KEY (orden_id, mueble_id)
            )
        )");

        W.commit();
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

void registrarMueble(connection &c) {
    limpiarConsola();
    cout << "REGISTRO DE MUEBLE" << endl;

    try {
        string nombre = obtenerTextoValido("┌─ Nombre del Mueble ────────────────┐\n│ > ", 2, 100);
        double precio = obtenerNumeroValido<double>("├─ Precio ($) ──────────────────────┐\n│ > ", 0.01, 999999.99);
        bool tiene_promo = false;
        cout << "├─ ¿Tiene promoción? (s/n) ─────────┐\n│ > ";
        string respuesta;
        getline(cin, respuesta);
        if (respuesta == "s" || respuesta == "S") tiene_promo = true;

        double alto = obtenerNumeroValido<double>("├─ Alto (cm) ───────────────────────┐\n│ > ", 1.0, 500.0);
        double ancho = obtenerNumeroValido<double>("├─ Ancho (cm) ──────────────────────┐\n│ > ", 1.0, 500.0);
        double profundidad = obtenerNumeroValido<double>("├─ Profundidad (cm) ────────────────┐\n│ > ", 1.0, 500.0);

        // Obtener maderas disponibles
        result maderas;
        {
            nontransaction N(c);
            maderas = N.exec("SELECT madera_id, nombre FROM Maderas ORDER BY nombre");
        }

        if (maderas.empty()) {
            cout << "\n┌────────────────────────────────────┐\n";
            cout << "│ ✘ No hay maderas registradas       │\n";
            cout << "│   Registre maderas primero         │\n";
            cout << "└────────────────────────────────────┘\n";
            pausa();
            return;
        }

        vector<int> maderas_seleccionadas;
        cout << "\n┌─ Maderas Disponibles ──────────────┐\n";
        for (const auto &row : maderas) {
            cout << "│ " << row["madera_id"].as<int>() << ". " << row["nombre"].as<string>() << endl;
        }
        cout << "├─ Seleccione maderas (0 para terminar)\n";

        while (true) {
            int madera_id = obtenerNumeroValido<int>("│ > ", 0, 999999);
            if (madera_id == 0) break;

            bool encontrada = false;
            for (const auto &row : maderas) {
                if (row["madera_id"].as<int>() == madera_id) {
                    encontrada = true;
                    if (find(maderas_seleccionadas.begin(), maderas_seleccionadas.end(), madera_id) != maderas_seleccionadas.end()) {
                        cout << "│ ✘ Esta madera ya fue seleccionada  │\n";
                    } else {
                        maderas_seleccionadas.push_back(madera_id);
                        cout << "│ ✔ Madera agregada                  │\n";
                    }
                    break;
                }
            }
            if (!encontrada) {
                cout << "│ ✘ Madera no encontrada             │\n";
            }
        }

        if (maderas_seleccionadas.empty()) {
            cout << "\n┌────────────────────────────────────┐\n";
            cout << "│ ✘ Debe seleccionar al menos una madera │\n";
            cout << "└────────────────────────────────────┘\n";
            pausa();
            return;
        }

        work W(c);
        // Insertar mueble
        result R = W.exec_params(
            "INSERT INTO Muebles (nombre, precio, tiene_promo, alto, ancho, profundidad) "
            "VALUES ($1, $2, $3, $4, $5, $6) RETURNING mueble_id",
            nombre, precio, tiene_promo, alto, ancho, profundidad
        );
        int mueble_id = R[0][0].as<int>();

        // Registrar relación con maderas
        for (int madera_id : maderas_seleccionadas) {
            W.exec_params(
                "INSERT INTO Muebles_Maderas (mueble_id, madera_id) VALUES ($1, $2)",
                mueble_id, madera_id
            );
        }

        W.commit();
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✔ Mueble registrado exitosamente   │\n";
        cout << "└────────────────────────────────────┘\n";
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ Error: " << e.what() << endl;
        cout << "└────────────────────────────────────┘\n";
    }
    pausa();
}

void consultarMuebles(connection &c) {
    limpiarConsola();
    cout <<"CONSULTA DE MUEBLES" << endl;

    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT m.mueble_id, m.nombre, m.precio, m.tiene_promo, m.alto, m.ancho, m.profundidad, "
            "string_agg(md.nombre, ', ') as maderas "
            "FROM Muebles m "
            "LEFT JOIN Muebles_Maderas mm ON m.mueble_id = mm.mueble_id "
            "LEFT JOIN Maderas md ON mm.madera_id = md.madera_id "
            "GROUP BY m.mueble_id, m.nombre, m.precio, m.tiene_promo, m.alto, m.ancho, m.profundidad "
            "ORDER BY m.mueble_id"
        );

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay muebles registrados       │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ ID: " << row["mueble_id"].as<int>() << endl;
                cout << "│ Nombre: " << row["nombre"].as<string>() << endl;
                cout << "│ Precio: $" << fixed << setprecision(2) << row["precio"].as<double>() << endl;
                cout << "│ Promo: " << (row["tiene_promo"].as<bool>() ? "Sí" : "No") << endl;
                cout << "│ Dimensiones: " << row["alto"].as<double>() << "x"
                     << row["ancho"].as<double>() << "x" << row["profundidad"].as<double>() << " cm\n";
                cout << "│ Maderas: " << (row["maderas"].is_null() ? "Ninguna" : row["maderas"].as<string>()) << endl;
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausa();
}

void registrarOrdenCompra(connection &c) {
    limpiarConsola();
    cout << "REGISTRO DE ORDEN DE COMPRA" << endl;
    try {
        // Obtener cliente
        string nombre_cliente = obtenerTextoValido("┌─ Nombre del Cliente ────────────────┐\n│ > ", 2, 100);
        string telefono = obtenerTextoValido("├─ Teléfono ─────────────────────────┐\n│ > ", 8, 20);
        string fecha_entrega = obtenerTextoValido("├─ Fecha de Entrega (YYYY-MM-DD) ────┐\n│ > ", 10, 10);
        if (!validarFecha(fecha_entrega)) {
            cout << "│ ✘ Fecha inválida                   │\n";
            pausa();
            return;
        }

        // Obtener dirección del cliente
        string calle = obtenerTextoValido("├─ Calle ────────────────────────────┐\n│ > ", 2, 100);
        string localidad = obtenerTextoValido("├─ Localidad ────────────────────────┐\n│ > ", 2, 100);
        string provincia = obtenerTextoValido("├─ Provincia ────────────────────────┐\n│ > ", 2, 100);

        // Obtener muebles disponibles
        result muebles;
        {
            nontransaction N(c);
            muebles = N.exec("SELECT mueble_id, nombre, precio FROM Muebles ORDER BY mueble_id");
        }
        if (muebles.empty()) {
            cout << "\n┌────────────────────────────────────┐\n";
            cout << "│ ✘ No hay muebles registrados       │\n";
            cout << "└────────────────────────────────────┘\n";
            pausa();
            return;
        }

        vector<pair<int, int>> muebles_seleccionados; // mueble_id, cantidad
        double total = 0.0;

        cout << "\n┌─ Muebles Disponibles ──────────────┐\n";
        for (const auto &row : muebles) {
            cout << "│ " << row["mueble_id"].as<int>() << ". " << row["nombre"].as<string>()
                 << " - $" << fixed << setprecision(2) << row["precio"].as<double>() << endl;
        }
        cout << "├─ Seleccione muebles (0 para terminar)\n";

        while (true) {
            int mueble_id = obtenerNumeroValido("│ ID Mueble: ", 0, 999999);
            if (mueble_id == 0) break;

            bool encontrado = false;
            double precio_mueble = 0.0;
            for (const auto &row : muebles) {
                if (row["mueble_id"].as<int>() == mueble_id) {
                    encontrado = true;
                    precio_mueble = row["precio"].as<double>();
                    break;
                }
            }

            if (!encontrado) {
                cout << "│ ✘ Mueble no encontrado             │\n";
                continue;
            }

            int cantidad = obtenerNumeroValido("│ Cantidad: ", 1, 100);
            muebles_seleccionados.emplace_back(mueble_id, cantidad);
            total += cantidad * precio_mueble;
            cout << "│ ✔ Mueble agregado                  │\n";
        }

        if (muebles_seleccionados.empty()) {
            cout << "\n┌────────────────────────────────────┐\n";
            cout << "│ ✘ Debe seleccionar al menos un mueble │\n";
            cout << "└────────────────────────────────────┘\n";
            pausa();
            return;
        }

        work W(c);

        // Insertar cliente
        W.exec_params(
            "INSERT INTO Clientes (nombre, telefono) VALUES ($1, $2)",
            nombre_cliente, telefono
        );

        // Obtener cliente_id usando currval
        result cliente_result = W.exec("SELECT currval('clientes_cliente_id_seq')");
        int cliente_id = cliente_result[0][0].as<int>();

        // Insertar orden de compra con dirección
        W.exec_params(
            "INSERT INTO OrdenesCompra (cliente_id, fecha, fecha_entrega, calle, localidad, provincia) "
            "VALUES ($1, CURRENT_DATE, $2, $3, $4, $5)",
            cliente_id, fecha_entrega, calle, localidad, provincia
        );

        // Obtener orden_id usando currval
        result orden_result = W.exec("SELECT currval('ordenescompra_orden_id_seq')");
        int orden_id = orden_result[0][0].as<int>();

        // Insertar detalle de la orden
        for (const auto &[mueble_id, cantidad] : muebles_seleccionados) {
            W.exec_params(
                "INSERT INTO DetalleOrden (orden_id, mueble_id, cantidad) VALUES ($1, $2, $3)",
                orden_id, mueble_id, cantidad
            );
        }

        W.commit();

        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✔ Orden registrada exitosamente    │\n";
        cout << "│ Total: $" << fixed << setprecision(2) << total << endl;
        cout << "└────────────────────────────────────┘\n";
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ Error: " << e.what() << endl;
        cout << "└────────────────────────────────────┘\n";
    }
    pausa();
}

void consultarOrdenesCompra(connection &c) {
    limpiarConsola();
    cout << "CONSULTA DE ÓRDENES DE COMPRA" << endl;

    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT o.orden_id, c.nombre AS cliente, c.telefono, "
            "o.fecha, o.fecha_entrega, o.calle, o.localidad, o.provincia, "
            "string_agg(m.nombre || ' (' || d.cantidad || ')', ', ') AS muebles "
            "FROM OrdenesCompra o "
            "JOIN Clientes c ON o.cliente_id = c.cliente_id "
            "LEFT JOIN DetalleOrden d ON o.orden_id = d.orden_id "
            "LEFT JOIN Muebles m ON d.mueble_id = m.mueble_id "
            "GROUP BY o.orden_id, c.nombre, c.telefono, o.fecha, o.fecha_entrega, o.calle, o.localidad, o.provincia "
            "ORDER BY o.orden_id DESC"
        );

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay órdenes registradas       │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ ID: " << row["orden_id"].as<int>() << endl;
                cout << "│ Cliente: " << row["cliente"].as<string>() << endl;
                cout << "│ Teléfono: " << row["telefono"].as<string>() << endl;
                cout << "│ Fecha: " << row["fecha"].as<string>() << endl;
                cout << "│ Entrega: " << row["fecha_entrega"].as<string>() << endl;
                cout << "│ Dirección: " << row["calle"].as<string>() << ", " 
                     << row["localidad"].as<string>() << ", " 
                     << row["provincia"].as<string>() << endl;
                cout << "│ Muebles: " << (row["muebles"].is_null() ? "Ninguno" : row["muebles"].as<string>()) << endl;
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausa();
}

void registrarProveedorMadera(connection &c) {
    limpiarConsola();
    cout << "REGISTRO DE PROVEEDOR DE MADERA" << endl;

    try {
        string nombre = obtenerTextoValido("┌─ Nombre del Proveedor ──────────────┐\n│ > ", 2, 100);
        string telefono = obtenerTextoValido("├─ Teléfono ─────────────────────────┐\n│ > ", 8, 20);

        work W(c);
        W.exec_params(
            "INSERT INTO ProveedoresMadera (nombre, telefono) VALUES ($1, $2)",
            nombre, telefono
        );
        W.commit();

        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✔ Proveedor registrado exitosamente│\n";
        cout << "└────────────────────────────────────┘\n";
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ Error: " << e.what() << endl;
        cout << "└────────────────────────────────────┘\n";
    }
    pausa();
}

void consultarProveedoresMadera(connection &c) {
    limpiarConsola();
    cout << "CONSULTA DE PROVEEDORES DE MADERA" << endl;

    try {
        nontransaction N(c);
        result R = N.exec("SELECT * FROM ProveedoresMadera ORDER BY proveedor_id");

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay proveedores registrados   │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ ID: " << row["proveedor_id"].as<int>() << endl;
                cout << "│ Nombre: " << row["nombre"].as<string>() << endl;
                cout << "│ Teléfono: " << row["telefono"].as<string>() << endl;
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausa();
}

void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarConsola();
        cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    MENÚ PRINCIPAL - MUEBLERÍA                ║
╚══════════════════════════════════════════════════════════════╝
┌──────────────────────────────────────────────────────────────┐
│ 1. Gestión de Muebles                                        │
│    ├─ Registrar nuevo mueble                                 │
│    └─ Consultar catálogo de muebles                          │
├──────────────────────────────────────────────────────────────┤
│ 2. Gestión de Órdenes de Compra                              │
│    ├─ Registrar nueva orden                                  │
│    └─ Consultar órdenes                                      │
├──────────────────────────────────────────────────────────────┤
│ 3. Gestión de Proveedores de Madera                          │
│    ├─ Registrar nuevo proveedor                              │
│    └─ Consultar proveedores                                  │
├──────────────────────────────────────────────────────────────┤
│ 0. Salir del sistema                                         │
└──────────────────────────────────────────────────────────────┘
)" << endl;

        opcion = obtenerNumeroValido("Seleccione una opción: ", 0, 3);
        try {
            switch (opcion) {
                case 1: {
                    limpiarConsola();
                    cout << R"(
┌──────────────────────────────────────────────────────┐
│                   GESTIÓN DE MUEBLES                 │
├──────────────────────────────────────────────────────┤
│ 1. Registrar nuevo mueble                            │
│ 2. Consultar catálogo de muebles                     │
└──────────────────────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarMueble(c);
                    else consultarMuebles(c);
                    break;
                }
                case 2: {
                    limpiarConsola();
                    cout << R"(
┌──────────────────────────────────────────────────────┐
│               GESTIÓN DE ÓRDENES DE COMPRA           │
├──────────────────────────────────────────────────────┤
│ 1. Registrar nueva orden                             │
│ 2. Consultar órdenes                                 │
└──────────────────────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarOrdenCompra(c);
                    else consultarOrdenesCompra(c);
                    break;
                }
                case 3: {
                    limpiarConsola();
                    cout << R"(
┌──────────────────────────────────────────────────────┐
│           GESTIÓN DE PROVEEDORES DE MADERA           │
├──────────────────────────────────────────────────────┤
│ 1. Registrar nuevo proveedor                         │
│ 2. Consultar proveedores                             │
└──────────────────────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarProveedorMadera(c);
                    else consultarProveedoresMadera(c);
                    break;
                }
                case 0:
                    limpiarConsola();
                    cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    ¡Gracias por usar el sistema!             ║
║                     Vuelva pronto                            ║
╚══════════════════════════════════════════════════════════════╝
)" << endl;
                    system("sleep 1");
                    break;
            }
        } catch (const exception &e) {
            cerr << "\n┌──────────────────────────────────────────────────────┐";
            cerr << "\n│ ✘ ERROR en la operación: " << e.what() << endl;
            cerr << "└──────────────────────────────────────────────────────┘\n";
            pausa();
        }
    } while (opcion != 0);
}

int main() {
    limpiarConsola();
    cout << R"(
╔═══════════════════════════════════════════════════════╗
║             SISTEMA DE GESTIÓN MUEBLERÍA              ║
║            Control Integral de Producción             ║
╚═══════════════════════════════════════════════════════╝
)" << endl;

    try {
        cout << "\n┌──────────────────────────────────────────────────────┐";
        cout << "\n│ ⚡ Iniciando sistema...                              │";
        connection c = iniciarConexion();
        cout << "\n│ ⚡ Verificando estructura de datos...                │";
        inicializarTablas(c);
        cout << "\n│ ✔ Sistema iniciado correctamente                     │";
        cout << "\n└──────────────────────────────────────────────────────┘\n";
        system("sleep 1");
        pausa();
        menuPrincipal(c);
    } catch (const exception &e) {
        cerr << "\n┌──────────────────────────────────────────────────────┐";
        cerr << "\n│ ✘ Error: " << e.what() << endl;
        cerr << "└──────────────────────────────────────────────────────┘\n";
        cout << "\nEl sistema se cerrará debido a un error.\n";
        pausa();
        return 1;
    }
    return 0;
}
