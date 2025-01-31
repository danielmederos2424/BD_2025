#include <iostream>
#include <pqxx/pqxx>
#include <string>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <iomanip>
#include <set>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void limpiarPantalla() {
    system("clear");
}

void pausar() {
    cout << "\nPresione Enter para continuar...";
    limpiarBuffer();
}

// Validaciones
bool validarDNI(const string& dni) {
    return dni.length() == 11 && all_of(dni.begin(), dni.end(), ::isdigit);
}

bool validarCUIT(const string& cuit) {
    return cuit.length() == 11 && all_of(cuit.begin(), cuit.end(), ::isdigit);
}

bool validarHora(const string& hora) {
    if (hora.length() != 5) return false;
    try {
        int h = stoi(hora.substr(0,2));
        int m = stoi(hora.substr(3,2));
        return hora[2] == ':' && h >= 0 && h < 24 && m >= 0 && m < 60;
    } catch (...) {
        return false;
    }
}

bool validarMonto(const string& monto) {
    try {
        double valor = stod(monto);
        return valor > 0;
    } catch (...) {
        return false;
    }
}

string obtenerEntrada(const string& mensaje, function<bool(const string&)> validador) {
    string entrada;
    do {
        cout << mensaje;
        getline(cin, entrada);
    } while (!validador(entrada));
    return entrada;
}

// Función de conexión
connection* conectarBaseDatos() {
    cout << "\n*========================================*";
    cout << "\n|      INICIANDO SISTEMA DEPORTES        |";
    cout << "\n*----------------------------------------*";
    
    try {
        const string CONFIG_DB = 
            "dbname=deportes "
            "user=postgres "
            "password=1234admin "
            "hostaddr=127.0.0.1 "
            "port=5432";

        cout << "\n| > Verificando configuración...         |";
        cout << "\n| > Intentando conexión a la base...     |";
        
        connection* conn = new connection(CONFIG_DB);
        
        if (!conn->is_open()) {
            cout << "\n| [ERROR] Conexión fallida             |";
            cout << "\n*========================================*\n";
            delete conn;
            throw runtime_error("No se pudo establecer la conexión");
        }

        cout << "\n| [OK] Conexión establecida              |";
        return conn;
    }
    catch(const exception& e) {
        cout << "\n| [ERROR] " << e.what();
        cout << "\n*========================================*\n";
        throw runtime_error("Error de conexión: " + string(e.what()));
    }
}


// Inicialización de tablas
void inicializarTablas(connection &conn) {
    work W(conn);
    
    // Sucursales
    W.exec("CREATE TABLE IF NOT EXISTS Sucursales ("
           "sucursal_id SERIAL PRIMARY KEY,"
           "domicilio VARCHAR(100) NOT NULL,"
           "ciudad VARCHAR(50) NOT NULL"
           ");");

    // Empleados
    W.exec("CREATE TABLE IF NOT EXISTS Empleados ("
           "legajo SERIAL PRIMARY KEY,"
           "nombre VARCHAR(100) NOT NULL,"
           "dni VARCHAR(11) UNIQUE NOT NULL CHECK (LENGTH(dni) = 11),"
           "calle VARCHAR(100) NOT NULL,"
           "numero INTEGER NOT NULL,"
           "ciudad VARCHAR(50) NOT NULL"
           ");");

    // Teléfonos de empleados
    W.exec("CREATE TABLE IF NOT EXISTS TelefonosEmpleados ("
           "legajo INTEGER REFERENCES Empleados(legajo),"
           "telefono VARCHAR(20) NOT NULL,"
           "PRIMARY KEY (legajo, telefono)"
           ");");

    // Asignación Empleados-Sucursales
    W.exec("CREATE TABLE IF NOT EXISTS EmpleadosSucursales ("
           "legajo INTEGER REFERENCES Empleados(legajo),"
           "sucursal_id INTEGER REFERENCES Sucursales(sucursal_id),"
           "PRIMARY KEY (legajo, sucursal_id)"
           ");");

    // Horarios de trabajo
    W.exec("CREATE TABLE IF NOT EXISTS HorariosEmpleados ("
           "legajo INTEGER,"
           "sucursal_id INTEGER,"
           "dia_semana INTEGER CHECK (dia_semana BETWEEN 1 AND 7),"
           "hora_inicio TIME NOT NULL,"
           "hora_fin TIME NOT NULL,"
           "PRIMARY KEY (legajo, sucursal_id, dia_semana),"
           "FOREIGN KEY (legajo, sucursal_id) REFERENCES EmpleadosSucursales(legajo, sucursal_id)"
           ");");

    // Fábricas
    W.exec("CREATE TABLE IF NOT EXISTS Fabricas ("
           "cuit VARCHAR(11) PRIMARY KEY CHECK (LENGTH(cuit) = 11),"
           "nombre VARCHAR(100) NOT NULL,"
           "pais_origen VARCHAR(50) NOT NULL,"
           "cantidad_empleados INTEGER NOT NULL CHECK (cantidad_empleados > 0),"
           "nombre_gerente VARCHAR(100) NOT NULL"
           ");");

    // Productos
    W.exec("CREATE TABLE IF NOT EXISTS Productos ("
           "codigo SERIAL PRIMARY KEY,"
           "descripcion VARCHAR(200) NOT NULL,"
           "color VARCHAR(50) NOT NULL,"
           "costo_fabricacion DECIMAL(10,2) NOT NULL CHECK (costo_fabricacion > 0),"
           "fabrica_cuit VARCHAR(11) REFERENCES Fabricas(cuit) UNIQUE"  // Una fábrica solo hace un tipo de producto
           ");");

    // Productos en sucursales con precio
    W.exec("CREATE TABLE IF NOT EXISTS ProductosSucursales ("
           "sucursal_id INTEGER REFERENCES Sucursales(sucursal_id),"
           "producto_codigo INTEGER REFERENCES Productos(codigo),"
           "precio_venta DECIMAL(10,2) NOT NULL CHECK (precio_venta > 0),"
           "PRIMARY KEY (sucursal_id, producto_codigo)"
           ");");

    // Clientes
    W.exec("CREATE TABLE IF NOT EXISTS Clientes ("
           "codigo SERIAL PRIMARY KEY,"
           "dni VARCHAR(11) UNIQUE NOT NULL CHECK (LENGTH(dni) = 11),"
           "nombre VARCHAR(100) NOT NULL,"
           "fecha_nacimiento DATE NOT NULL,"
           "ciudad VARCHAR(50) NOT NULL"
           ");");

    // Relación Clientes-Sucursales con descuento
    W.exec("CREATE TABLE IF NOT EXISTS ClientesSucursales ("
           "cliente_codigo INTEGER REFERENCES Clientes(codigo),"
           "sucursal_id INTEGER REFERENCES Sucursales(sucursal_id),"
           "descuento_fidelidad DECIMAL(5,2) NOT NULL CHECK (descuento_fidelidad >= 0 AND descuento_fidelidad <= 100),"
           "PRIMARY KEY (cliente_codigo, sucursal_id)"
           ");");

    // Tarjetas de crédito
    W.exec("CREATE TABLE IF NOT EXISTS TarjetasCredito ("
           "cliente_codigo INTEGER REFERENCES Clientes(codigo),"
           "nombre_tarjeta VARCHAR(50) NOT NULL,"
           "numero VARCHAR(16) NOT NULL,"
           "codigo_seguridad VARCHAR(4) NOT NULL,"
           "fecha_vencimiento DATE NOT NULL,"
           "PRIMARY KEY (numero)"
           ");");

    // Ventas
    W.exec("CREATE TABLE IF NOT EXISTS Ventas ("
           "venta_id SERIAL PRIMARY KEY,"
           "cliente_codigo INTEGER,"
           "sucursal_id INTEGER,"
           "fecha DATE NOT NULL DEFAULT CURRENT_DATE,"
           "total DECIMAL(10,2) NOT NULL CHECK (total > 0),"
           "FOREIGN KEY (cliente_codigo, sucursal_id) REFERENCES ClientesSucursales(cliente_codigo, sucursal_id)"
           ");");

    // Detalle de ventas
    W.exec("CREATE TABLE IF NOT EXISTS DetalleVentas ("
           "venta_id INTEGER REFERENCES Ventas(venta_id),"
           "producto_codigo INTEGER,"
           "sucursal_id INTEGER,"
           "cantidad INTEGER NOT NULL CHECK (cantidad > 0),"
           "precio_unitario DECIMAL(10,2) NOT NULL CHECK (precio_unitario > 0),"
           "FOREIGN KEY (sucursal_id, producto_codigo) REFERENCES ProductosSucursales(sucursal_id, producto_codigo),"
           "PRIMARY KEY (venta_id, producto_codigo)"
           ");");

    W.commit();
}

void registrarEmpleado(connection &conn) {
    limpiarPantalla();
    cout << "\n*========================================*";
    cout << "\n|         REGISTRO DE EMPLEADO           |";
    cout << "\n*========================================*\n";
    
    try {
        work W(conn);
        
        // Verificar si existen sucursales
        result check_suc = W.exec("SELECT 1 FROM Sucursales LIMIT 1");
        if (check_suc.empty()) {
            cout << "\n| [ERROR] No hay sucursales registradas   |";
            cout << "\n| Debe registrar sucursales primero      |";
            cout << "\n*========================================*\n";
            pausar();
            return;
        }
        
        // Validar y obtener datos básicos
        string dni = obtenerEntrada("DNI (11 dígitos): ", validarDNI);
        
        // Verificar si el DNI ya existe
        result R = W.exec_params("SELECT 1 FROM Empleados WHERE dni = $1", dni);
        if (!R.empty()) {
            cout << "\n| [ERROR] Ya existe un empleado con ese DNI |";
            cout << "\n*========================================*\n";
            pausar();
            return;
        }
        
        string nombre;
        do {
            cout << "Nombre completo: ";
            getline(cin, nombre);
            if (nombre.empty() || nombre.length() > 100) {
                cout << "| [ERROR] Nombre inválido (1-100 caracteres) |" << endl;
            }
        } while (nombre.empty() || nombre.length() > 100);
        
        string calle;
        do {
            cout << "Calle: ";
            getline(cin, calle);
            if (calle.empty() || calle.length() > 100) {
                cout << "| [ERROR] Calle inválida (1-100 caracteres) |" << endl;
            }
        } while (calle.empty() || calle.length() > 100);
        
        int numero;
        do {
            cout << "Número: ";
            string input;
            getline(cin, input);
            try {
                numero = stoi(input);
                if (numero <= 0) {
                    cout << "| [ERROR] El número debe ser mayor a 0    |" << endl;
                    continue;
                }
                break;
            } catch (...) {
                cout << "| [ERROR] Número inválido                 |" << endl;
            }
        } while (true);
        
        string ciudad;
        do {
            cout << "Ciudad: ";
            getline(cin, ciudad);
            if (ciudad.empty() || ciudad.length() > 50) {
                cout << "| [ERROR] Ciudad inválida (1-50 caracteres) |" << endl;
            }
        } while (ciudad.empty() || ciudad.length() > 50);

        // Insertar empleado y obtener legajo
        result emp = W.exec_params(
            "INSERT INTO Empleados (nombre, dni, calle, numero, ciudad) "
            "VALUES ($1, $2, $3, $4, $5) RETURNING legajo",
            nombre, dni, calle, numero, ciudad);
        
        int legajo = emp[0][0].as<int>();

        // Registrar teléfonos
        cout << "\n*----------------------------------------*";
        cout << "\n| REGISTRO DE TELÉFONOS                  |";
        cout << "\n*----------------------------------------*\n";
        cout << "| Ingrese teléfonos (0 para terminar)     |" << endl;
        
        set<string> telefonos;
        while (true) {
            cout << "Teléfono: ";
            string telefono;
            getline(cin, telefono);
            
            if (telefono == "0") {
                if (telefonos.empty()) {
                    cout << "| [ERROR] Debe registrar al menos un tel. |" << endl;
                    continue;
                }
                break;
            }
            
            if (telefono.length() < 8 || !all_of(telefono.begin(), telefono.end(), ::isdigit)) {
                cout << "| [ERROR] Teléfono inválido              |" << endl;
                continue;
            }
            
            if (telefonos.find(telefono) != telefonos.end()) {
                cout << "| [ERROR] Teléfono ya registrado         |" << endl;
                continue;
            }
            
            telefonos.insert(telefono);
            W.exec_params(
                "INSERT INTO TelefonosEmpleados (legajo, telefono) VALUES ($1, $2)",
                legajo, telefono);
                
            cout << "| [OK] Teléfono registrado               |" << endl;
        }

        // Asignar sucursales y horarios
        cout << "\n*----------------------------------------*";
        cout << "\n| ASIGNACIÓN DE SUCURSALES Y HORARIOS    |";
        cout << "\n*----------------------------------------*\n";

        // Mostrar sucursales disponibles
        result sucursales = W.exec(
            "SELECT sucursal_id, domicilio, ciudad FROM Sucursales ORDER BY sucursal_id");
            
        cout << "\nSucursales disponibles:\n";
        for (const auto &row : sucursales) {
            cout << "ID: " << row["sucursal_id"].as<int>() 
                 << " - " << row["ciudad"].as<string>()
                 << " (" << row["domicilio"].as<string>() << ")\n";
        }
        
        set<pair<int,int>> horarios_registrados;
        while (true) {
            cout << "\nSucursal ID (0 para terminar): ";
            string input_suc;
            getline(cin, input_suc);
            
            if (input_suc == "0") {
                if (horarios_registrados.empty()) {
                    cout << "| [ERROR] Debe asignar al menos un horario |" << endl;
                    continue;
                }
                break;
            }
            
            int sucursal_id;
            try {
                sucursal_id = stoi(input_suc);
            } catch (...) {
                cout << "| [ERROR] ID de sucursal inválido        |" << endl;
                continue;
            }
            
            // Verificar si la sucursal existe
            result suc = W.exec_params(
                "SELECT 1 FROM Sucursales WHERE sucursal_id = $1", 
                sucursal_id);
            
            if (suc.empty()) {
                cout << "| [ERROR] La sucursal no existe          |" << endl;
                continue;
            }
            
            // Registrar asignación empleado-sucursal
            W.exec_params(
                "INSERT INTO EmpleadosSucursales (legajo, sucursal_id) "
                "VALUES ($1, $2) ON CONFLICT DO NOTHING",
                legajo, sucursal_id);
            
            // Registrar horarios para esta sucursal
            cout << "\nIngrese horarios para esta sucursal:\n";
            bool horario_registrado = false;
            
            while (true) {
                cout << "Día (1-7, 0 para terminar): ";
                string input_dia;
                getline(cin, input_dia);
                
                try {
                    int dia = stoi(input_dia);
                    if (dia == 0) {
                        if (!horario_registrado) {
                            cout << "| [ERROR] Debe registrar al menos un horario |" << endl;
                            continue;
                        }
                        break;
                    }
                    if (dia < 1 || dia > 7) {
                        cout << "| [ERROR] Día inválido (1-7)             |" << endl;
                        continue;
                    }
                    
                    if (horarios_registrados.find({sucursal_id, dia}) != horarios_registrados.end()) {
                        cout << "| [ERROR] Ya existe horario para este día |" << endl;
                        continue;
                    }
                    
                    string hora_inicio = obtenerEntrada("Hora inicio (HH:MM): ", validarHora);
                    string hora_fin = obtenerEntrada("Hora fin (HH:MM): ", validarHora);
                    
                    W.exec_params(
                        "INSERT INTO HorariosEmpleados "
                        "(legajo, sucursal_id, dia_semana, hora_inicio, hora_fin) "
                        "VALUES ($1, $2, $3, $4, $5)",
                        legajo, sucursal_id, dia, hora_inicio, hora_fin);
                    
                    horarios_registrados.insert({sucursal_id, dia});
                    horario_registrado = true;
                    cout << "| [OK] Horario registrado                |" << endl;
                    
                } catch (...) {
                    cout << "| [ERROR] Entrada inválida              |" << endl;
                    continue;
                }
            }
        }

        W.commit();
        cout << "\n*========================================*";
        cout << "\n| [OK] Empleado registrado               |";
        cout << "\n| Legajo asignado: " << setw(3) << legajo << "                  |";
        cout << "\n*========================================*\n";
    }
    catch (const exception &e) {
        cout << "\n*========================================*";
        cout << "\n| [ERROR] " << e.what();
        cout << "\n*========================================*\n";
    }
    
    pausar();
}

void registrarProducto(connection &conn) {
    limpiarPantalla();
    cout << "\n=== REGISTRO DE PRODUCTO Y FÁBRICA ===\n";
    
    try {
        work W(conn);
        
        // Registrar/validar fábrica
        string cuit = obtenerEntrada("CUIT Fábrica (11 dígitos): ", validarCUIT);
        
        result R = W.exec_params("SELECT nombre FROM Fabricas WHERE cuit = $1", cuit);
        
        if (R.empty()) {
            cout << "\nNueva fábrica - complete los datos:\n";
            
            string nombre;
            do {
                cout << "Nombre: ";
                getline(cin, nombre);
            } while (nombre.empty() || nombre.length() > 100);
            
            string pais;
            do {
                cout << "País de origen: ";
                getline(cin, pais);
            } while (pais.empty() || pais.length() > 50);
            
            int cant_empleados;
            do {
                cout << "Cantidad de empleados: ";
                string input;
                getline(cin, input);
                try {
                    cant_empleados = stoi(input);
                    if (cant_empleados > 0) break;
                } catch (...) {}
                cout << "Error: Ingrese un número válido mayor a 0\n";
            } while (true);
            
            string gerente;
            do {
                cout << "Nombre del gerente: ";
                getline(cin, gerente);
            } while (gerente.empty() || gerente.length() > 100);
            
            // Verificar si la fábrica ya tiene productos asignados
            result check = W.exec_params(
                "SELECT 1 FROM Productos WHERE fabrica_cuit = $1", 
                cuit);
            
            if (!check.empty()) {
                cout << "Error: Esta fábrica ya tiene un producto asignado\n";
                return;
            }
            
            W.exec_params(
                "INSERT INTO Fabricas (cuit, nombre, pais_origen, cantidad_empleados, nombre_gerente) "
                "VALUES ($1, $2, $3, $4, $5)",
                cuit, nombre, pais, cant_empleados, gerente);
        } else {
            cout << "Fábrica existente: " << R[0][0].as<string>() << endl;
            
            // Verificar si ya tiene producto asignado
            result check = W.exec_params(
                "SELECT 1 FROM Productos WHERE fabrica_cuit = $1", 
                cuit);
            
            if (!check.empty()) {
                cout << "Error: Esta fábrica ya tiene un producto asignado\n";
                return;
            }
        }
        
        // Registrar producto
        cout << "\nDatos del producto:\n";
        
        string descripcion;
        do {
            cout << "Descripción: ";
            getline(cin, descripcion);
        } while (descripcion.empty() || descripcion.length() > 200);
        
        string color;
        do {
            cout << "Color: ";
            getline(cin, color);
        } while (color.empty() || color.length() > 50);
        
        string costo = obtenerEntrada("Costo de fabricación: ", validarMonto);
        
        result prod = W.exec_params(
            "INSERT INTO Productos (descripcion, color, costo_fabricacion, fabrica_cuit) "
            "VALUES ($1, $2, $3, $4) RETURNING codigo",
            descripcion, color, costo, cuit);
        
        int codigo = prod[0][0].as<int>();
        
        // Asignar precios por sucursal
        cout << "\nAsignar precios por sucursal (ID 0 para terminar):\n";
        while (true) {
            cout << "\nSucursal ID (0 para terminar): ";
            string input_suc;
            getline(cin, input_suc);
            
            if (input_suc == "0") break;
            
            int sucursal_id;
            try {
                sucursal_id = stoi(input_suc);
            } catch (...) {
                cout << "Error: ID de sucursal inválido\n";
                continue;
            }
            
            // Verificar si la sucursal existe
            result suc = W.exec_params(
                "SELECT 1 FROM Sucursales WHERE sucursal_id = $1", 
                sucursal_id);
            
            if (suc.empty()) {
                cout << "Error: La sucursal no existe\n";
                continue;
            }
            
            string precio = obtenerEntrada("Precio de venta: ", validarMonto);
            
            W.exec_params(
                "INSERT INTO ProductosSucursales (sucursal_id, producto_codigo, precio_venta) "
                "VALUES ($1, $2, $3)",
                sucursal_id, codigo, precio);
        }
        
        W.commit();
        cout << "\nProducto registrado exitosamente con código: " << codigo << endl;
    }
    catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    
    pausar();
}

void consultarProductosSucursal(connection &conn) {
    limpiarPantalla();
    cout << "\n=== CONSULTA DE PRODUCTOS POR SUCURSAL ===\n";
    
    try {
        // Mostrar sucursales disponibles
        nontransaction N(conn);
        result sucursales = N.exec("SELECT sucursal_id, domicilio, ciudad FROM Sucursales ORDER BY sucursal_id");
        
        if (sucursales.empty()) {
            cout << "No hay sucursales registradas en el sistema.\n";
            pausar();
            return;
        }
        
        cout << "\nSucursales disponibles:\n";
        for (const auto &row : sucursales) {
            cout << "ID: " << row["sucursal_id"].as<int>()
                 << " - " << row["ciudad"].as<string>()
                 << " (" << row["domicilio"].as<string>() << ")\n";
        }
        
        // Solicitar y validar ID de sucursal
        int sucursal_id;
        do {
            cout << "\nIngrese ID de sucursal (0 para cancelar): ";
            string input;
            getline(cin, input);
            
            try {
                sucursal_id = stoi(input);
                if (sucursal_id == 0) return;
                
                result check = N.exec_params(
                    "SELECT 1 FROM Sucursales WHERE sucursal_id = $1",
                    sucursal_id);
                
                if (!check.empty()) break;
                cout << "Error: Sucursal no encontrada\n";
            } catch (...) {
                cout << "Error: ID inválido\n";
            }
        } while (true);
        
        // Consultar productos con todos los detalles
        result productos = N.exec_params(
            "SELECT p.codigo, p.descripcion, p.color, p.costo_fabricacion, "
            "ps.precio_venta, f.nombre as fabricante, f.pais_origen "
            "FROM ProductosSucursales ps "
            "JOIN Productos p ON ps.producto_codigo = p.codigo "
            "JOIN Fabricas f ON p.fabrica_cuit = f.cuit "
            "WHERE ps.sucursal_id = $1 "
            "ORDER BY p.descripcion",
            sucursal_id);
            
        if (productos.empty()) {
            cout << "\nNo hay productos registrados en esta sucursal.\n";
        } else {
            cout << "\nProductos encontrados:\n";
            cout << string(50, '=') << endl;
            
            double total_valor = 0;
            int total_productos = 0;
            
            for (const auto &row : productos) {
                total_productos++;
                total_valor += row["precio_venta"].as<double>();
                
                cout << "Código: " << row["codigo"].as<int>() << "\n"
                     << "Descripción: " << row["descripcion"].as<string>() << "\n"
                     << "Color: " << row["color"].as<string>() << "\n"
                     << "Costo fabricación: $" << fixed << setprecision(2) 
                     << row["costo_fabricacion"].as<double>() << "\n"
                     << "Precio venta: $" << row["precio_venta"].as<double>() << "\n"
                     << "Fabricante: " << row["fabricante"].as<string>() << "\n"
                     << "País origen: " << row["pais_origen"].as<string>() << "\n"
                     << string(50, '-') << endl;
            }
            
            cout << "\nResumen:"
                 << "\nTotal productos: " << total_productos
                 << "\nValor total inventario: $" << fixed << setprecision(2) << total_valor
                 << endl;
        }
    }
    catch (const exception &e) {
        cout << "\nError: " << e.what() << endl;
    }
    
    pausar();
}

void consultarHorariosEmpleados(connection &conn) {
    limpiarPantalla();
    cout << "\n*========================================*";
    cout << "\n|    CONSULTA DE HORARIOS EMPLEADOS      |";
    cout << "\n*========================================*\n";
    
    try {
        nontransaction N(conn);
        
        // Verificar si existen sucursales
        result check_suc = N.exec("SELECT 1 FROM Sucursales LIMIT 1");
        if (check_suc.empty()) {
            cout << "\n| [ERROR] No hay sucursales registradas  |";
            cout << "\n| Debe registrar sucursales primero      |";
            cout << "\n*========================================*\n";
            pausar();
            return;
        }
        
        // Mostrar sucursales disponibles
        result sucursales = N.exec(
            "SELECT sucursal_id, ciudad, domicilio "
            "FROM Sucursales "
            "ORDER BY sucursal_id");
            
        cout << "\nSucursales disponibles:\n";
        for (const auto &row : sucursales) {
            cout << "ID: " << row["sucursal_id"].as<int>() 
                 << " - " << row["ciudad"].as<string>()
                 << " (" << row["domicilio"].as<string>() << ")\n";
        }

        // Validar y obtener sucursal
        int sucursal_id;
        do {
            cout << "\nID Sucursal (0 para cancelar): ";
            string input;
            getline(cin, input);
            
            try {
                sucursal_id = stoi(input);
                if (sucursal_id == 0) return;
                
                result check = N.exec_params(
                    "SELECT ciudad, domicilio FROM Sucursales WHERE sucursal_id = $1",
                    sucursal_id);
                
                if (!check.empty()) {
                    cout << "\n*----------------------------------------*";
                    cout << "\n| Sucursal: " << check[0]["ciudad"].as<string>();
                    cout << "\n| " << check[0]["domicilio"].as<string>();
                    cout << "\n*----------------------------------------*\n";
                    break;
                }
                cout << "| [ERROR] Sucursal no encontrada         |" << endl;
            } catch (...) {
                cout << "| [ERROR] ID inválido                    |" << endl;
            }
        } while (true);

        // Obtener horarios por día
        const string dias[] = {"Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo"};
        bool hay_horarios = false;
        
        for (int dia = 1; dia <= 7; dia++) {
            result horarios = N.exec_params(
                "SELECT e.legajo, e.nombre, e.dni, h.hora_inicio, h.hora_fin "
                "FROM HorariosEmpleados h "
                "JOIN Empleados e ON h.legajo = e.legajo "
                "WHERE h.sucursal_id = $1 AND h.dia_semana = $2 "
                "ORDER BY h.hora_inicio",
                sucursal_id, dia);
            
            cout << "\n" << dias[dia-1] << ":\n";
            cout << string(40, '-') << endl;
            
            if (horarios.empty()) {
                cout << "| Sin empleados asignados\n";
            } else {
                hay_horarios = true;
                for (const auto &row : horarios) {
                    cout << "| Legajo: " << row["legajo"].as<int>() << endl
                         << "| " << row["nombre"].as<string>() << endl
                         << "| DNI: " << row["dni"].as<string>() << endl
                         << "| Horario: " << row["hora_inicio"].as<string>() 
                         << " a " << row["hora_fin"].as<string>() << endl
                         << string(40, '-') << endl;
                }
            }
        }
        
        if (!hay_horarios) {
            cout << "\n*----------------------------------------*";
            cout << "\n| [INFO] No hay horarios registrados     |";
            cout << "\n*----------------------------------------*";
        }
        
        cout << "\n*========================================*\n";
    }
    catch (const exception &e) {
        cout << "\n*========================================*";
        cout << "\n| [ERROR] " << e.what();
        cout << "\n*========================================*\n";
    }
    
    pausar();
}

void mostrarMenu() {
    limpiarPantalla();
    cout << "\n*========================================*";
    cout << "\n|        CADENA DE DEPORTES v1.0         |";
    cout << "\n*========================================*";
    cout << "\n|                                        |";
    cout << "\n|  1. GESTIÓN DE EMPLEADOS               |";
    cout << "\n|     > Registrar empleado               |";
    cout << "\n|     > Consultar horarios               |";
    cout << "\n|                                        |";
    cout << "\n|  2. GESTIÓN DE PRODUCTOS               |";
    cout << "\n|     > Registrar producto/fábrica       |";
    cout << "\n|     > Consultar productos              |";
    cout << "\n|                                        |";
    cout << "\n|  3. CONSULTAS DEL SISTEMA              |";
    cout << "\n|     > Horarios por sucursal            |";
    cout << "\n|     > Productos y precios              |";
    cout << "\n|                                        |";
    cout << "\n|  0. SALIR                              |";
    cout << "\n|                                        |";
    cout << "\n*========================================*\n";
}

int main() {
    connection* conn = nullptr;
    limpiarPantalla();
    
    try {
        // Inicialización del sistema
        conn = conectarBaseDatos();
        
        cout << "\n| > Verificando estructura de datos...   |";
        inicializarTablas(*conn);
        
        cout << "\n| [OK] Sistema listo                     |";
        cout << "\n*========================================*\n";
        system("sleep 1");
        pausar();

        // Bucle principal del programa
        int opcion;
        string entrada;
        
        do {
            mostrarMenu();
            cout << "\nSeleccione una opción: ";
            getline(cin, entrada);
            
            try {
                opcion = stoi(entrada);
            } catch(...) {
                opcion = -1;
            }
            
            switch(opcion) {
                case 1: {
                    limpiarPantalla();
                    cout << "\n*========================================*";
                    cout << "\n|         GESTIÓN DE EMPLEADOS           |";
                    cout << "\n*========================================*";
                    cout << "\n| 1. Registrar empleado                  |";
                    cout << "\n| 2. Consultar horarios                  |";
                    cout << "\n| 0. Volver al menú principal            |";
                    cout << "\n*========================================*\n";
                    cout << "\nSeleccione una opción: ";
                    
                    getline(cin, entrada);
                    if(entrada == "1") registrarEmpleado(*conn);
                    else if(entrada == "2") consultarHorariosEmpleados(*conn);
                    break;
                }
                    
                case 2: {
                    limpiarPantalla();
                    cout << "\n*========================================*";
                    cout << "\n|         GESTIÓN DE PRODUCTOS           |";
                    cout << "\n*========================================*";
                    cout << "\n| 1. Registrar producto                  |";
                    cout << "\n| 2. Consultar productos                 |";
                    cout << "\n| 0. Volver al menú principal            |";
                    cout << "\n*========================================*\n";
                    cout << "\nSeleccione una opción: ";
                    
                    getline(cin, entrada);
                    if(entrada == "1") registrarProducto(*conn);
                    else if(entrada == "2") consultarProductosSucursal(*conn);
                    break;
                }
                    
                case 3: {
                    limpiarPantalla();
                    cout << "\n*========================================*";
                    cout << "\n|         CONSULTAS DEL SISTEMA          |";
                    cout << "\n*========================================*";
                    cout << "\n| 1. Consultar horarios por sucursal     |";
                    cout << "\n| 2. Consultar productos y precios       |";
                    cout << "\n| 0. Volver al menú principal            |";
                    cout << "\n*========================================*\n";
                    cout << "\nSeleccione una opción: ";
                    
                    getline(cin, entrada);
                    if(entrada == "1") consultarHorariosEmpleados(*conn);
                    else if(entrada == "2") consultarProductosSucursal(*conn);
                    break;
                }
                    
                case 0: {
                    limpiarPantalla();
                    cout << "\n*========================================*";
                    cout << "\n|          FINALIZANDO SISTEMA           |";
                    cout << "\n|                                        |";
                    cout << "\n|          ¡Hasta pronto!                |";
                    cout << "\n*========================================*\n";
                    system("sleep 1");
                    break;
                }
                    
                default: {
                    cout << "\n[ERROR] Opción inválida";
                    pausar();
                }
            }
        } while(opcion != 0);
        
        delete conn;
        return 0;
    }
    catch(const exception& e) {
        cout << "\n*========================================*";
        cout << "\n| [ERROR CRÍTICO]                        |";
        cout << "\n| " << e.what();
        cout << "\n*========================================*\n";
        cout << "\nEl sistema se cerrará.\n";
        if(conn) delete conn;
        pausar();
        return 1;
    }
}
