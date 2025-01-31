#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <vector>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include <set>
#include <iomanip>
#include <ctime>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarConsola() {
    system("clear");
}

void limpiarBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausarPrograma() {
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
bool esNumeroTelefono(const string& str) {
    return !str.empty() && all_of(str.begin(), str.end(), ::isdigit);
}

bool esCIValido(const string& ci) {
    return ci.length() == 11 && all_of(ci.begin(), ci.end(), ::isdigit);
}

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

// Funciones de validación para la base de datos
bool existeCliente(connection &c, const string& ci) {
    try {
        nontransaction N(c);
        result R = N.exec_params(
            "SELECT cliente_id FROM Clientes WHERE numero_documento = $1",
            ci
        );
        return !R.empty();
    } catch (const exception &e) {
        return false;
    }
}

bool existeCajon(connection &c, const string& nombre) {
    try {
        nontransaction N(c);
        result R = N.exec_params(
            "SELECT cajon_id FROM Cajones WHERE nombre = $1",
            nombre
        );
        return !R.empty();
    } catch (const exception &e) {
        return false;
    }
}

bool existeProducto(connection &c, int producto_id) {
    try {
        nontransaction N(c);
        result R = N.exec_params(
            "SELECT producto_id FROM Productos WHERE producto_id = $1",
            producto_id
        );
        return !R.empty();
    } catch (const exception &e) {
        return false;
    }
}

bool validarFecha(const string& fecha) {
    if (fecha.length() != 10) return false;
    
    // Verificar formato YYYY-MM-DD
    if (fecha[4] != '-' || fecha[7] != '-') return false;
    
    try {
        int year = stoi(fecha.substr(0, 4));
        int month = stoi(fecha.substr(5, 2));
        int day = stoi(fecha.substr(8, 2));
        
        if (year < 2024 || year > 2100) return false;
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;
        
        // Validar días según el mes
        if (month == 4 || month == 6 || month == 9 || month == 11) {
            if (day > 30) return false;
        } else if (month == 2) {
            bool bisiesto = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            if (day > (bisiesto ? 29 : 28)) return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool validarNumeroCuenta(const string& cuenta) {
    if (cuenta.length() < 10 || cuenta.length() > 50) return false;
    
    // Verificar que solo contenga números, letras, guiones y espacios
    return all_of(cuenta.begin(), cuenta.end(), [](char c) {
        return isalnum(c) || c == '-' || c == ' ';
    });
}

// Funciones de entrada
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

// Conexión a la base de datos
connection iniciarConexion() {
    const string CONFIGURACION_DB = 
        "dbname=pedidos "
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
        
        // Tabla Clientes
        W.exec("CREATE TABLE IF NOT EXISTS Clientes ("
               "cliente_id SERIAL PRIMARY KEY,"
               "numero_documento VARCHAR(11) UNIQUE NOT NULL CHECK (LENGTH(numero_documento) = 11),"
               "nombre VARCHAR(100) NOT NULL,"
               "apellidos VARCHAR(100) NOT NULL"
               ");");

        // Tabla Teléfonos
        W.exec("CREATE TABLE IF NOT EXISTS Telefonos ("
               "cliente_id INTEGER REFERENCES Clientes(cliente_id),"
               "numero VARCHAR(20) NOT NULL CHECK (numero ~ '^[0-9]+$'),"
               "PRIMARY KEY (cliente_id, numero)"
               ");");

        // Tabla Proveedores
        W.exec("CREATE TABLE IF NOT EXISTS Proveedores ("
               "proveedor_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) NOT NULL,"
               "domicilio TEXT NOT NULL,"
               "dias_entrega TEXT[] NOT NULL"
               ");");

        // Tabla Productos
        W.exec("CREATE TABLE IF NOT EXISTS Productos ("
               "producto_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) NOT NULL,"
               "precio_kg NUMERIC(10,2) NOT NULL CHECK (precio_kg > 0),"
               "proveedor_id INTEGER REFERENCES Proveedores(proveedor_id),"
               "UNIQUE(nombre, proveedor_id)"
               ");");

        // Tabla Cajones
        W.exec("CREATE TABLE IF NOT EXISTS Cajones ("
               "cajon_id SERIAL PRIMARY KEY,"
               "nombre VARCHAR(100) UNIQUE NOT NULL,"
               "descripcion TEXT NOT NULL"
               ");");

        // Tabla Productos_Cajon
        W.exec("CREATE TABLE IF NOT EXISTS Productos_Cajon ("
               "cajon_id INTEGER REFERENCES Cajones(cajon_id),"
               "producto_id INTEGER REFERENCES Productos(producto_id),"
               "cantidad_kg NUMERIC(10,2) NOT NULL CHECK (cantidad_kg > 0),"
               "PRIMARY KEY (cajon_id, producto_id)"
               ");");

        // Tabla Pedidos
        W.exec("CREATE TABLE IF NOT EXISTS Pedidos ("
               "pedido_id SERIAL PRIMARY KEY,"
               "cliente_id INTEGER REFERENCES Clientes(cliente_id),"
               "fecha_pedido DATE NOT NULL DEFAULT CURRENT_DATE,"
               "fecha_entrega DATE NOT NULL,"
               "forma_pago VARCHAR(20) NOT NULL CHECK (forma_pago IN ('efectivo', 'transferencia'))"
               ");");

        // Tabla Pedidos_Cajones
        W.exec("CREATE TABLE IF NOT EXISTS Pedidos_Cajones ("
               "pedido_id INTEGER REFERENCES Pedidos(pedido_id),"
               "cajon_id INTEGER REFERENCES Cajones(cajon_id),"
               "PRIMARY KEY (pedido_id, cajon_id)"
               ");");

        // Tabla Detalle_Pago (solo para transferencias)
        W.exec("CREATE TABLE IF NOT EXISTS Detalle_Pago ("
               "pedido_id INTEGER PRIMARY KEY REFERENCES Pedidos(pedido_id),"
               "cuenta_origen VARCHAR(50) NOT NULL"
               ");");

        W.commit();
    } catch (const exception &e) {
        cerr << "Error durante la inicialización de tablas: " << e.what() << endl;
        throw;
    }
}

// Funciones de registro
void registrarCliente(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║        REGISTRO DE CLIENTE           ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        // Obtener y validar CI
        string ci;
        bool ci_valido = false;
        do {
            cout << "┌─ Carnet de Identidad (11 dígitos) ─┐\n│ > ";
            getline(cin, ci);
            
            if (!esCIValido(ci)) {
                cout << "│ ✘ CI inválido. Debe tener 11 dígitos numéricos\n└────────────────────────────────────┘\n";
                continue;
            }
            
            // Verificar si el CI ya existe antes de intentar insertar
            if (existeCliente(c, ci)) {
                cout << "│ ✘ Ya existe un cliente con ese CI\n└────────────────────────────────────┘\n";
                continue;
            }
            
            ci_valido = true;
        } while (!ci_valido);

        // Obtener nombre y apellidos
        string nombre = obtenerTextoValido("┌─ Nombre ─────────────────────────┐\n│ > ", 2, 100);
        string apellidos = obtenerTextoValido("├─ Apellidos ──────────────────────┐\n│ > ", 2, 100);

        work W(c);
        
        // Registrar cliente
        W.exec_params("INSERT INTO Clientes (numero_documento, nombre, apellidos) VALUES ($1, $2, $3)",
                     ci, nombre, apellidos);
        
        // Registrar teléfonos
        cout << "\n┌─ Teléfonos ────────────────────────┐";
        cout << "\n│ Ingrese teléfonos (0 para terminar)│\n";
        
        set<string> telefonos_registrados;  // Para evitar duplicados locales
        string telefono;
        bool al_menos_un_telefono = false;
        
        while (true) {
            cout << "│ > ";
            getline(cin, telefono);
            
            if (telefono == "0") {
                if (!al_menos_un_telefono) {
                    cout << "│ ℹ Debe registrar al menos un teléfono\n";
                    continue;
                }
                break;
            }
            
            if (!esNumeroTelefono(telefono)) {
                cout << "│ ✘ Número inválido. Solo dígitos permitidos\n";
                continue;
            }
            
            if (telefonos_registrados.find(telefono) != telefonos_registrados.end()) {
                cout << "│ ✘ Este número ya fue registrado\n";
                continue;
            }
            
            telefonos_registrados.insert(telefono);
            W.exec_params("INSERT INTO Telefonos (cliente_id, numero) "
                        "SELECT currval('clientes_cliente_id_seq'), $1",
                        telefono);
            al_menos_un_telefono = true;
            cout << "│ ✔ Teléfono registrado\n";
        }
        
        W.commit();
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✔ Cliente registrado exitosamente  │\n";
        cout << "└────────────────────────────────────┘\n";
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ No se pudo completar el registro │\n";
        cout << "│   Intente nuevamente               │\n";
        cout << "└────────────────────────────────────┘\n";
    }
    pausarPrograma();
}

void registrarCajon(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║         REGISTRO DE CAJÓN            ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        // Obtener productos disponibles en un bloque de transacción separado
        result productos;
        {
            nontransaction N(c);
            productos = N.exec(
                "SELECT p.producto_id, p.nombre, pr.nombre as proveedor, p.precio_kg, pr.proveedor_id "
                "FROM Productos p "
                "JOIN Proveedores pr ON p.proveedor_id = pr.proveedor_id "
                "ORDER BY p.nombre, pr.nombre"
            );
        }

        if (productos.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ✘ No hay productos registrados     │\n";
            cout << "│   Registre productos primero       │\n";
            cout << "└────────────────────────────────────┘\n";
            pausarPrograma();
            return;
        }

        string nombre;
        string descripcion;
        vector<tuple<int, double, int>> productos_seleccionados; // producto_id, cantidad, proveedor_id
        double total_peso = 0.0;

        // Validar nombre del cajón
        bool nombre_valido = false;
        do {
            nombre = obtenerTextoValido("┌─ Nombre del Cajón ─────────────────┐\n│ > ", 3, 100);
            
            bool existe = false;
            {
                nontransaction N(c);
                result R = N.exec_params(
                    "SELECT 1 FROM Cajones WHERE nombre = $1",
                    nombre
                );
                existe = !R.empty();
            }
            
            if (existe) {
                cout << "│ ✘ Ya existe un cajón con ese nombre\n└────────────────────────────────────┘\n";
                continue;
            }
            nombre_valido = true;
        } while (!nombre_valido);

        descripcion = obtenerTextoValido("├─ Descripción ─────────────────────┐\n│ > ", 10, 500);

        // Mostrar productos disponibles
        cout << "\n┌─ Productos Disponibles ──────────────┐\n";
        for (const auto& row : productos) {
            cout << "│ " << row[0].as<int>() << ". " << row[1].as<string>() 
                 << " (" << row[2].as<string>() << ")\n│    $" 
                 << fixed << setprecision(2) << row[3].as<double>() << "/kg\n";
        }

        // Recolectar productos para el cajón
        set<int> productos_agregados;
        bool al_menos_un_producto = false;

        while (true) {
            int producto_id = obtenerNumeroValido<int>("│\n│ ID Producto (0 para terminar): ", 0, 999999);
            
            if (producto_id == 0) {
                if (!al_menos_un_producto) {
                    cout << "│ ℹ Debe agregar al menos un producto │\n";
                    continue;
                }
                break;
            }

            if (productos_agregados.find(producto_id) != productos_agregados.end()) {
                cout << "│ ✘ Este producto ya fue agregado    │\n";
                continue;
            }

            bool producto_encontrado = false;
            for (const auto& row : productos) {
                if (row[0].as<int>() == producto_id) {
                    producto_encontrado = true;
                    
                    double cantidad;
                    bool cantidad_valida = false;
                    do {
                        cantidad = obtenerNumeroValido<double>("│ Cantidad en kg (0.01-50.00): ", 0.01, 50.00);
                        if (total_peso + cantidad > 100.0) {
                            cout << "│ ✘ El peso total excedería 100 kg  │\n";
                            continue;
                        }
                        cantidad_valida = true;
                    } while (!cantidad_valida);

                    productos_seleccionados.push_back({producto_id, cantidad, row[4].as<int>()});
                    productos_agregados.insert(producto_id);
                    total_peso += cantidad;
                    al_menos_un_producto = true;
                    
                    cout << "│ ✔ Producto agregado               │\n";
                    cout << "│   Peso total actual: " << fixed << setprecision(2) << total_peso << " kg      │\n";
                    break;
                }
            }
            
            if (!producto_encontrado) {
                cout << "│ ✘ Producto no encontrado          │\n";
            }
        }

        // Realizar la inserción en una única transacción
        {
            work W(c);
            try {
                // Insertar cajón
                result R = W.exec_params(
                    "INSERT INTO Cajones (nombre, descripcion) VALUES ($1, $2) RETURNING cajon_id",
                    nombre, descripcion
                );
                
                int cajon_id = R[0][0].as<int>();

                // Insertar productos verificando existencia
                for (const auto& [prod_id, cantidad, prov_id] : productos_seleccionados) {
                    result verify = W.exec_params(
                        "SELECT 1 FROM Productos WHERE producto_id = $1 AND proveedor_id = $2",
                        prod_id, prov_id
                    );
                    
                    if (verify.empty()) {
                        throw runtime_error("Producto no válido o proveedor incorrecto");
                    }
                    
                    W.exec_params(
                        "INSERT INTO Productos_Cajon (cajon_id, producto_id, cantidad_kg) "
                        "VALUES ($1, $2, $3)",
                        cajon_id, prod_id, cantidad
                    );
                }
                
                W.commit();
                cout << "\n┌────────────────────────────────────┐\n";
                cout << "│ ✔ Cajón registrado exitosamente    │\n";
                cout << "└────────────────────────────────────┘\n";
            } catch (const exception& e) {
                W.abort();
                throw runtime_error(string("Error en la transacción: ") + e.what());
            }
        }
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ Error: " << e.what() << endl;
        cout << "└────────────────────────────────────┘\n";
    }
    pausarPrograma();
}

void registrarPedido(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║         REGISTRO DE PEDIDO           ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        // Verificar cajones disponibles en una transacción separada
        result cajones;
        {
            nontransaction N(c);
            cajones = N.exec(
                "SELECT c.cajon_id, c.nombre, c.descripcion, "
                "SUM(pc.cantidad_kg * p.precio_kg) as precio_total "
                "FROM Cajones c "
                "LEFT JOIN Productos_Cajon pc ON c.cajon_id = pc.cajon_id "
                "LEFT JOIN Productos p ON pc.producto_id = p.producto_id "
                "GROUP BY c.cajon_id, c.nombre, c.descripcion "
                "ORDER BY c.nombre"
            );
        }

        if (cajones.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ✘ No hay cajones registrados       │\n";
            cout << "│   Registre cajones primero         │\n";
            cout << "└────────────────────────────────────┘\n";
            pausarPrograma();
            return;
        }

        // Estructura para almacenar datos del pedido
        struct DatosPedido {
            int cliente_id;
            string fecha_entrega;
            string forma_pago;
            string cuenta_origen;
            vector<pair<int, double>> cajones_seleccionados; // <cajon_id, precio>
        } datos_pedido;

        // Verificar cliente
        string ci;
        result cliente;
        bool cliente_encontrado = false;
        
        do {
            cout << "┌─ CI del Cliente (11 dígitos) ──────┐\n│ > ";
            getline(cin, ci);
            
            if (!esCIValido(ci)) {
                cout << "│ ✘ CI inválido                      │\n";
                continue;
            }
            
            // Consultar cliente en una transacción separada
            {
                nontransaction N(c);
                cliente = N.exec_params(
                    "SELECT cliente_id, nombre, apellidos FROM Clientes "
                    "WHERE numero_documento = $1",
                    ci
                );
            }
            
            if (cliente.empty()) {
                cout << "│ ✘ Cliente no encontrado            │\n";
                cout << "│   ¿Desea registrarlo? (s/n): ";
                string respuesta;
                getline(cin, respuesta);
                if (respuesta == "s" || respuesta == "S") {
                    registrarCliente(c);
                    return;
                }
            } else {
                cout << "│ ✔ Cliente: " << cliente[0]["nombre"].as<string>() 
                     << " " << cliente[0]["apellidos"].as<string>() << endl;
                datos_pedido.cliente_id = cliente[0]["cliente_id"].as<int>();
                cliente_encontrado = true;
            }
        } while (!cliente_encontrado);

        // Validar forma de pago
        cout << "\n┌─ Forma de Pago ────────────────────┐\n";
        cout << "│ 1. Efectivo                       │\n";
        cout << "│ 2. Transferencia                  │\n";
        int forma_pago_opt = obtenerNumeroValido<int>("│ Seleccione: ", 1, 2);
        datos_pedido.forma_pago = (forma_pago_opt == 1) ? "efectivo" : "transferencia";
        
        if (datos_pedido.forma_pago == "transferencia") {
            bool cuenta_valida = false;
            do {
                datos_pedido.cuenta_origen = obtenerTextoValido(
                    "├─ Número de Cuenta ─────────────────┐\n│ > ", 
                    10, 50
                );
                if (!validarNumeroCuenta(datos_pedido.cuenta_origen)) {
                    cout << "│ ✘ Número de cuenta inválido        │\n";
                } else {
                    cuenta_valida = true;
                }
            } while (!cuenta_valida);
        }

        // Validar fecha de entrega
        bool fecha_valida = false;
        
        // Obtener y formatear la fecha actual correctamente
        time_t now = time(0);
        tm* ltm = localtime(&now);
        stringstream ss;
        ss << (1900 + ltm->tm_year) << "-"
           << setw(2) << setfill('0') << (1 + ltm->tm_mon) << "-"
           << setw(2) << setfill('0') << ltm->tm_mday;
        string fecha_actual = ss.str();

        do {
            datos_pedido.fecha_entrega = obtenerTextoValido(
                "├─ Fecha de Entrega (YYYY-MM-DD) ────┐\n│ > ", 
                10, 10
            );
            if (!validarFecha(datos_pedido.fecha_entrega)) {
                cout << "│ ✘ Fecha inválida                   │\n";
                continue;
            }

            // Convertir strings a tm para comparar fechas correctamente
            tm fecha_entrega = {};
            tm fecha_hoy = {};
            stringstream ss_entrega(datos_pedido.fecha_entrega);
            stringstream ss_hoy(fecha_actual);
            
            ss_entrega >> get_time(&fecha_entrega, "%Y-%m-%d");
            ss_hoy >> get_time(&fecha_hoy, "%Y-%m-%d");
            
            if (mktime(&fecha_entrega) <= mktime(&fecha_hoy)) {
                cout << "│ ✘ La fecha debe ser posterior a hoy│\n";
                continue;
            }
            
            fecha_valida = true;
        } while (!fecha_valida);

        // Mostrar cajones disponibles
        cout << "\n┌─ Cajones Disponibles ──────────────┐\n";
        for (const auto& row : cajones) {
            cout << "│ " << row["cajon_id"].as<int>() << ". " 
                 << row["nombre"].as<string>() << endl;
            cout << "│    " << row["descripcion"].as<string>() << endl;
            cout << "│    Precio: $" << fixed << setprecision(2) 
                 << row["precio_total"].as<double>() << endl;
            cout << "├────────────────────────────────────┤\n";
        }

        // Seleccionar cajones para el pedido
        cout << "\n│ Seleccione los cajones (0 para terminar)\n";
        set<int> cajones_agregados;
        double total_pedido = 0.0;
        bool al_menos_un_cajon = false;

        while (true) {
            int cajon_id = obtenerNumeroValido<int>("│ > ", 0, 999999);
            if (cajon_id == 0) {
                if (!al_menos_un_cajon) {
                    cout << "│ ✘ Debe seleccionar al menos un cajón│\n";
                    continue;
                }
                break;
            }

            if (cajones_agregados.find(cajon_id) != cajones_agregados.end()) {
                cout << "│ ✘ Este cajón ya fue agregado       │\n";
                continue;
            }

            bool cajon_encontrado = false;
            for (const auto& row : cajones) {
                if (row["cajon_id"].as<int>() == cajon_id) {
                    double precio = row["precio_total"].as<double>();
                    
                    // Confirmar selección mostrando el precio
                    cout << "│ Precio del cajón: $" << fixed << setprecision(2) 
                         << precio << endl;
                    cout << "│ ¿Confirmar selección? (s/n): ";
                    string confirma;
                    getline(cin, confirma);
                    
                    if (confirma == "s" || confirma == "S") {
                        datos_pedido.cajones_seleccionados.push_back({cajon_id, precio});
                        cajones_agregados.insert(cajon_id);
                        total_pedido += precio;
                        al_menos_un_cajon = true;
                        
                        cout << "│ ✔ Cajón agregado                 │\n";
                        cout << "│   Total actual: $" << fixed << setprecision(2) 
                             << total_pedido << endl;
                    }
                    cajon_encontrado = true;
                    break;
                }
            }

            if (!cajon_encontrado) {
                cout << "│ ✘ Cajón no encontrado             │\n";
            }
        }

        // Realizar la inserción en una única transacción
        {
            work W(c);
            try {
                // Verificar nuevamente la disponibilidad de los cajones
                for (const auto& [cajon_id, precio] : datos_pedido.cajones_seleccionados) {
                    result verify = W.exec_params(
                        "SELECT 1 FROM Cajones WHERE cajon_id = $1",
                        cajon_id
                    );
                    if (verify.empty()) {
                        throw runtime_error("Uno de los cajones seleccionados ya no está disponible");
                    }
                }

                // Insertar pedido
                W.exec_params(
                    "INSERT INTO Pedidos (cliente_id, fecha_entrega, forma_pago) "
                    "VALUES ($1, $2, $3)",
                    datos_pedido.cliente_id, 
                    datos_pedido.fecha_entrega, 
                    datos_pedido.forma_pago
                );

                // Si es transferencia, registrar detalles
                if (datos_pedido.forma_pago == "transferencia") {
                    W.exec_params(
                        "INSERT INTO Detalle_Pago (pedido_id, cuenta_origen) "
                        "VALUES (currval('pedidos_pedido_id_seq'), $1)",
                        datos_pedido.cuenta_origen
                    );
                }

                // Registrar cajones seleccionados
                for (const auto& [cajon_id, precio] : datos_pedido.cajones_seleccionados) {
                    W.exec_params(
                        "INSERT INTO Pedidos_Cajones (pedido_id, cajon_id) "
                        "VALUES (currval('pedidos_pedido_id_seq'), $1)",
                        cajon_id
                    );
                }

                W.commit();
                cout << "\n┌────────────────────────────────────┐\n";
                cout << "│ ✔ Pedido registrado exitosamente   │\n";
                cout << "│   Total a pagar: $" << fixed << setprecision(2) 
                     << total_pedido << endl;
                cout << "└────────────────────────────────────┘\n";
            } catch (const exception& e) {
                W.abort();
                throw runtime_error(string("Error en la transacción: ") + e.what());
            }
        }
    } catch (const exception &e) {
        cout << "\n┌────────────────────────────────────┐\n";
        cout << "│ ✘ Error: " << e.what() << endl;
        cout << "└────────────────────────────────────┘\n";
    }
    pausarPrograma();
}

// Funciones de consulta
void consultarClientes(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║       CONSULTA DE CLIENTES           ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT c.numero_documento, c.nombre, c.apellidos, "
            "string_agg(t.numero, ', ') as telefonos, "
            "COUNT(DISTINCT p.pedido_id) as total_pedidos "
            "FROM Clientes c "
            "LEFT JOIN Telefonos t ON c.cliente_id = t.cliente_id "
            "LEFT JOIN Pedidos p ON c.cliente_id = p.cliente_id "
            "GROUP BY c.cliente_id, c.numero_documento, c.nombre, c.apellidos "
            "ORDER BY c.apellidos, c.nombre;"
        );

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay clientes registrados      │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ CI: " << row["numero_documento"].as<string>() << endl;
                cout << "│ Nombre: " << row["nombre"].as<string>() 
                     << " " << row["apellidos"].as<string>() << endl;
                cout << "│ Teléfonos: " << 
                    (row["telefonos"].is_null() ? "Ninguno" : row["telefonos"].as<string>()) << endl;
                cout << "│ Total Pedidos: " << row["total_pedidos"].as<int>() << endl;
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausarPrograma();
}

void consultarCajones(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║         CONSULTA DE CAJONES          ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT c.cajon_id, c.nombre, c.descripcion, "
            "COUNT(DISTINCT pc.producto_id) as total_productos, "
            "SUM(pc.cantidad_kg * p.precio_kg) as precio_total "
            "FROM Cajones c "
            "LEFT JOIN Productos_Cajon pc ON c.cajon_id = pc.cajon_id "
            "LEFT JOIN Productos p ON pc.producto_id = p.producto_id "
            "GROUP BY c.cajon_id, c.nombre, c.descripcion "
            "ORDER BY c.nombre;"
        );

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay cajones registrados       │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ " << row["nombre"].as<string>() << endl;
                cout << "│ Descripción: " << row["descripcion"].as<string>() << endl;
                cout << "│ Productos diferentes: " << row["total_productos"].as<int>() << endl;
                
                // Obtener detalle de productos
                result productos = N.exec_params(
                    "SELECT p.nombre, pc.cantidad_kg, p.precio_kg, pr.nombre as proveedor "
                    "FROM Productos_Cajon pc "
                    "JOIN Productos p ON pc.producto_id = p.producto_id "
                    "JOIN Proveedores pr ON p.proveedor_id = pr.proveedor_id "
                    "WHERE pc.cajon_id = $1 "
                    "ORDER BY p.nombre;",
                    row["cajon_id"].as<int>()
                );
                
                if (!productos.empty()) {
                    cout << "│ Contenido:\n";
                    for (const auto &prod : productos) {
                        cout << "│  - " << prod["nombre"].as<string>() 
                             << " (" << prod["proveedor"].as<string>() << ")\n"
                             << "│    " << prod["cantidad_kg"].as<double>() << "kg x $"
                             << prod["precio_kg"].as<double>() << "/kg\n";
                    }
                }
                
                cout << "│ Precio Total: $" << row["precio_total"].as<double>() << endl;
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausarPrograma();
}

void consultarPedidos(connection &c) {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║         CONSULTA DE PEDIDOS          ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        nontransaction N(c);
        result R = N.exec(
            "SELECT p.pedido_id, c.numero_documento, c.nombre, c.apellidos, "
            "p.fecha_pedido, p.fecha_entrega, p.forma_pago, "
            "COUNT(DISTINCT pc.cajon_id) as total_cajones "
            "FROM Pedidos p "
            "JOIN Clientes c ON p.cliente_id = c.cliente_id "
            "LEFT JOIN Pedidos_Cajones pc ON p.pedido_id = pc.pedido_id "
            "GROUP BY p.pedido_id, c.numero_documento, c.nombre, c.apellidos, "
            "p.fecha_pedido, p.fecha_entrega, p.forma_pago "
            "ORDER BY p.fecha_pedido DESC, p.pedido_id DESC;"
        );

        if (R.empty()) {
            cout << "┌────────────────────────────────────┐\n";
            cout << "│ ℹ No hay pedidos registrados       │\n";
            cout << "└────────────────────────────────────┘\n";
        } else {
            for (const auto &row : R) {
                cout << "┌────────────────────────────────────┐\n";
                cout << "│ Pedido #" << row["pedido_id"].as<int>() << endl;
                cout << "│ Cliente: " << row["nombre"].as<string>() 
                     << " " << row["apellidos"].as<string>() << endl;
                cout << "│ CI: " << row["numero_documento"].as<string>() << endl;
                cout << "│ Fecha Pedido: " << row["fecha_pedido"].as<string>() << endl;
                cout << "│ Fecha Entrega: " << row["fecha_entrega"].as<string>() << endl;
                cout << "│ Forma Pago: " << row["forma_pago"].as<string>() << endl;
                
                if (row["forma_pago"].as<string>() == "transferencia") {
                    result pago = N.exec_params(
                        "SELECT cuenta_origen FROM Detalle_Pago WHERE pedido_id = $1",
                        row["pedido_id"].as<int>()
                    );
                    if (!pago.empty()) {
                        cout << "│ Cuenta: " << pago[0]["cuenta_origen"].as<string>() << endl;
                    }
                }
                
                // Obtener detalle de cajones
                result cajones = N.exec_params(
                    "SELECT c.nombre, c.descripcion "
                    "FROM Pedidos_Cajones pc "
                    "JOIN Cajones c ON pc.cajon_id = c.cajon_id "
                    "WHERE pc.pedido_id = $1 "
                    "ORDER BY c.nombre;",
                    row["pedido_id"].as<int>()
                );
                
                cout << "│ Cajones:\n";
                for (const auto &cajon : cajones) {
                    cout << "│  - " << cajon["nombre"].as<string>() << endl;
                }
                cout << "└────────────────────────────────────┘\n";
            }
        }
    } catch (const exception &e) {
        cerr << "\n✘ Error: " << e.what() << endl;
    }
    pausarPrograma();
}

// Menú principal
void menuPrincipal(connection &c) {
    int opcion;
    do {
        limpiarConsola();
        cout << R"(
╔══════════════════════════════════════╗
║     SISTEMA DE GESTIÓN DE PEDIDOS    ║
╚══════════════════════════════════════╝

┌────────────────────────────────────┐
│ 1. Gestión de Clientes             │
│    ├─ Registrar cliente            │
│    └─ Consultar clientes           │
├────────────────────────────────────┤
│ 2. Gestión de Cajones              │
│    ├─ Registrar cajón              │
│    └─ Consultar cajones            │
├────────────────────────────────────┤
│ 3. Gestión de Pedidos              │
│    ├─ Registrar pedido             │
│    └─ Consultar pedidos            │
├────────────────────────────────────┤
│ 0. Salir                           │
└────────────────────────────────────┘
)" << endl;

        opcion = obtenerNumeroValido<int>("Seleccione una opción: ", 0, 3);

        try {
            switch (opcion) {
                case 1: {
                    limpiarConsola();
                    cout << R"(
┌────────────────────────────────────┐
│ 1. Registrar Cliente               │
│ 2. Consultar Clientes              │
└────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido<int>("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarCliente(c);
                    else consultarClientes(c);
                    break;
                }
                case 2: {
                    limpiarConsola();
                    cout << R"(
┌────────────────────────────────────┐
│ 1. Registrar Cajón                 │ 
│ 2. Consultar Cajones               │
└────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido<int>("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarCajon(c);
                    else consultarCajones(c);
                    break;
                }
                case 3: {
                    limpiarConsola();
                    cout << R"(
┌────────────────────────────────────┐
│ 1. Registrar Pedido                │ 
│ 2. Consultar Pedidos               │
└────────────────────────────────────┘
)" << endl;
                    int subopcion = obtenerNumeroValido<int>("Seleccione: ", 1, 2);
                    if (subopcion == 1) registrarPedido(c);
                    else consultarPedidos(c);
                    break;
                }
                case 0:
                    limpiarConsola();
                    cout << R"(
╔══════════════════════════════════════╗
║    ¡Gracias por usar el sistema!     ║
╚══════════════════════════════════════╝
)" << endl;
                    system("sleep 1");
                    break;
            }
        } catch (const exception &e) {
            cerr << "\n✘ ERROR en la operación: " << e.what() << endl;
            pausarPrograma();
        }
    } while (opcion != 0);
}

int main() {
    limpiarConsola();
    cout << R"(
╔══════════════════════════════════════╗
║      SISTEMA DE GESTIÓN v1.0         ║
║         PEDIDOS CLIENTES             ║
╚══════════════════════════════════════╝
)" << endl;
    
    try {
        cout << "\n┌─ Iniciando Sistema ─────────────────┐";
        cout << "\n│ ⚡ Conectando con base de datos...  │";
        connection c = iniciarConexion();
        
        cout << "\n│ ⚡ Verificando estructura de datos  │";
        inicializarTablas(c);
        
        cout << "\n│ ✔ Sistema iniciado correctamente    │";
        cout << "\n└─────────────────────────────────────┘\n";
        system("sleep 1");
        
        pausarPrograma();
        menuPrincipal(c);
        
    } catch (const exception &e) {
        cerr << "\n┌────────────────────────────────────┐";
        cerr << "\n│ ✘ Error crítico: " << e.what() << endl;
        cerr << "└────────────────────────────────────┘\n";
        cout << "\nEl sistema se cerrará debido a un error.\n";
        pausarPrograma();
        return 1;
    }
    
    return 0;
}