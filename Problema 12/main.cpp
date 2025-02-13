#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <limits>
#include <string>
#include <algorithm>
#include <regex>
#include <vector>
#include <iomanip>

using namespace std;
using namespace pqxx;

// Funciones auxiliares
void limpiarPantalla() { system("clear"); }

void limpiarBuffer(){
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void pausa(){
    cout << "\nPresione Enter para continuar...";
    cin.get();
}

void ejecutarConsulta(connection &c, const string &query){
    try
    {
        work W(c);
        W.exec(query);
        W.commit();
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError SQL: " << e.what() << "\033[0m\n";
        throw;
    }
}

connection conectar(){
    try
    {
        connection c("dbname=inmobiliaria user=postgres password=1234admin hostaddr=127.0.0.1 port=5432");
        if (!c.is_open())
            throw runtime_error("No se pudo establecer conexión");
        return c;
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError de conexión: " << e.what() << "\033[0m\n";
        throw;
    }
}

void crearTablasInmobiliaria(connection &c){
    try
    {
        // Tabla Oficinas
        ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Oficinas ("
                            "ID SERIAL PRIMARY KEY,"
                            "Nombre VARCHAR(100),"
                            "Direccion VARCHAR(200),"
                            "Telefono VARCHAR(20));");

        // Tabla Inmuebles actualizada
        ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Inmuebles ("
                            "Ref SERIAL PRIMARY KEY,"
                            "Tipo VARCHAR(50),"
                            "Superficie INTEGER CHECK (Superficie > 0),"
                            "Direccion VARCHAR(100),"
                            "Zona VARCHAR(50),"
                            "PrecioVenta FLOAT,"
                            "PrecioAlquiler FLOAT,"
                            "Propietario VARCHAR(100),"
                            "TelefonoPropietario VARCHAR(20),"
                            "OficinaID INTEGER REFERENCES Oficinas(ID),"
                            "TieneLlaves BOOLEAN DEFAULT false);");

        // Tabla CaracteristicasVivienda
        ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS CaracteristicasVivienda ("
                            "Ref INTEGER PRIMARY KEY REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                            "NumHabitaciones INTEGER,"
                            "NumBanos INTEGER,"
                            "NumAseos INTEGER,"
                            "NumCocinas INTEGER,"
                            "Altura INTEGER,"
                            "TieneGasCiudad BOOLEAN,"
                            "TienePuertaBlindada BOOLEAN,"
                            "TieneParquet BOOLEAN,"
                            "TieneCalefaccion BOOLEAN);");

        // Tabla CaracteristicasLocal
        ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS CaracteristicasLocal ("
                            "Ref INTEGER PRIMARY KEY REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                            "Diafano BOOLEAN,"
                            "NumPuertas INTEGER,"
                            "TieneAireAcondicionado BOOLEAN,"
                            "SuperficieAltillo FLOAT);");

        // Tabla Visitas
        ejecutarConsulta(c, "CREATE TABLE IF NOT EXISTS Visitas ("
                            "ID SERIAL PRIMARY KEY,"
                            "RefInmueble INTEGER REFERENCES Inmuebles(Ref) ON DELETE CASCADE,"
                            "FechaHora TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                            "Cliente VARCHAR(100),"
                            "Comentario TEXT);");
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError al crear tablas: " << e.what() << "\033[0m\n";
        throw;
    }
}

// Validaciones
template <typename T>
T validarEntradaNumerica(const string &mensaje, T minimo = 0, T maximo = numeric_limits<T>::max()){
    T valor;
    while (true)
    {
        cout << mensaje;
        if (cin >> valor)
        {
            if (valor >= minimo && valor <= maximo)
            {
                limpiarBuffer();
                return valor;
            }
            cout << "\033[1;31mValor fuera de rango. Debe estar entre "
                << minimo << " y " << fixed << setprecision(0) << maximo << ".\033[0m\n";
             }
        else
        {
            cout << "\033[1;31mEntrada inválida. Introduzca un número válido.\033[0m\n";
            limpiarBuffer();
        }
    }
}

string validarEntradaTexto(const string &mensaje, int minLongitud = 1, int maxLongitud = 100){
    string texto;
    while (true)
    {
        cout << mensaje;
        getline(cin, texto);

        // Eliminar espacios al inicio y final
        texto.erase(0, texto.find_first_not_of(" \t"));
        texto.erase(texto.find_last_not_of(" \t") + 1);

        if (!texto.empty() && texto.length() >= minLongitud && texto.length() <= maxLongitud)
        {
            return texto;
        }

        cout << "\033[1;31mEntrada inválida. La longitud debe estar entre "
             << minLongitud << " y " << maxLongitud << " caracteres.\033[0m\n";
    }
}

bool validarEntradaBooleana(const string &mensaje){
    char respuesta;
    while (true)
    {
        cout << mensaje << " (s/n): ";
        cin >> respuesta;
        limpiarBuffer();

        respuesta = tolower(respuesta);
        if (respuesta == 's')
            return true;
        if (respuesta == 'n')
            return false;

        cout << "\033[1;31mRespuesta inválida. Use 's' o 'n'.\033[0m\n";
    }
}

string formatearPrecio(float precio){
    if (precio == 0)
        return "No disponible";

    ostringstream ss;
    ss.imbue(locale(""));
    ss << fixed << setprecision(2) << precio;
    return "€" + ss.str();
}

void agregarInmueble(connection &c){
    limpiarPantalla();
    try
    {
        work W(c);

        // Validación de oficina
        result R = W.exec("SELECT ID, Nombre FROM Oficinas ORDER BY ID;");
        if (R.empty())
        {
            cout << "\033[1;31mNo hay oficinas registradas. Registre una oficina primero.\033[0m\n";
            pausa();
            return;
        }

        cout << "Oficinas disponibles:\n";
        for (auto row : R)
        {
            cout << row[0].as<int>() << ". " << row[1].as<string>() << "\n";
        }

        // Validar selección de oficina
        int maxOficinaId = R[R.size() - 1][0].as<int>();
        int oficinaId = validarEntradaNumerica<int>("Seleccione oficina: ", 1, maxOficinaId);

        // Nueva implementación para seleccionar tipo de inmueble
        cout << "\nTipos de inmueble disponibles:\n"
             << "1. Piso\n"
             << "2. Casa\n"
             << "3. Villa\n"
             << "4. Local\n";

        int opcionTipo = validarEntradaNumerica<int>("Seleccione tipo: ", 1, 4);
        string tipo;
        switch (opcionTipo)
        {
        case 1:
            tipo = "Piso";
            break;
        case 2:
            tipo = "Casa";
            break;
        case 3:
            tipo = "Villa";
            break;
        case 4:
            tipo = "Local";
            break;
        }

        // Resto del código permanece igual
        int superficie = validarEntradaNumerica<int>("Superficie (m2): ", 1, 50000);
        string direccion = validarEntradaTexto("Dirección (ej. Calle Mayor 10, 2º A): ", 5, 100);
        string zona = validarEntradaTexto("Zona: ", 3, 50);
        float precioVenta = validarEntradaNumerica<float>("Precio venta (0 si no está en venta): ", 0, 50000000.0);
        float precioAlquiler = validarEntradaNumerica<float>("Precio alquiler (0 si no está en alquiler): ", 0, 100000.0);
        string propietario = validarEntradaTexto("Nombre del propietario: ", 3, 100);

        // Validar teléfono con formato específico
        string telefono;
        while (true)
        {
            telefono = validarEntradaTexto("Teléfono (formato: +34 612 345 678 o 612345678): ", 9, 20);

            // Eliminar espacios y el prefijo +34 si existe
            string telLimpio = telefono;
            telLimpio.erase(remove(telLimpio.begin(), telLimpio.end(), ' '), telLimpio.end());
            if (telLimpio.substr(0, 3) == "+34")
            {
                telLimpio = telLimpio.substr(3);
            }

            // Verificar que sean 9 dígitos y empiece con 6,7,8 o 9
            if (telLimpio.length() == 9 &&
                all_of(telLimpio.begin(), telLimpio.end(), ::isdigit) &&
                (telLimpio[0] >= '6' && telLimpio[0] <= '9'))
            {
                telefono = telLimpio; // Guardar el número en formato limpio
                break;
            }

            cout << "\033[1;31mFormato de teléfono inválido. Debe ser un número de 9 dígitos que empiece por 6, 7, 8 o 9.\033[0m\n";
        }

        bool tieneLlaves = validarEntradaBooleana("¿Tiene llaves?");

        // Insertar inmueble
        string query = "INSERT INTO Inmuebles (Tipo, Superficie, Direccion, Zona, "
                       "PrecioVenta, PrecioAlquiler, Propietario, TelefonoPropietario, "
                       "OficinaID, TieneLlaves) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
                       "RETURNING Ref;";

        result res = W.exec_params(query,
                                   tipo, superficie, direccion, zona,
                                   precioVenta, precioAlquiler, propietario, telefono,
                                   oficinaId, tieneLlaves);

        int refInmueble = res[0][0].as<int>();

        // Características específicas según tipo
        if (tipo != "Local")
        {
            int numHab = validarEntradaNumerica<int>("Número de habitaciones: ", 1, 20);
            int numBanos = validarEntradaNumerica<int>("Número de baños: ", 1, 10);
            int numAseos = validarEntradaNumerica<int>("Número de aseos: ", 0, 5);
            int numCocinas = validarEntradaNumerica<int>("Número de cocinas: ", 1, 3);
            bool gasCity = validarEntradaBooleana("¿Tiene gas ciudad?");
            bool puertaBlin = validarEntradaBooleana("¿Tiene puerta blindada?");
            bool parquet = validarEntradaBooleana("¿Tiene parquet?");
            bool calef = validarEntradaBooleana("¿Tiene calefacción?");

            W.exec_params("INSERT INTO CaracteristicasVivienda VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)",
                          refInmueble, numHab, numBanos, numAseos, numCocinas,
                          0, // Altura (no solicitada en la versión original)
                          gasCity, puertaBlin, parquet, calef);
        }
        else
        {
            bool diafano = validarEntradaBooleana("¿Es diáfano?");
            int numPuertas = validarEntradaNumerica<int>("Número de puertas: ", 1, 10);
            bool aire = validarEntradaBooleana("¿Tiene aire acondicionado?");
            float supAltillo = validarEntradaNumerica<float>("Superficie del altillo (m2): ", 0, 1000.0);

            W.exec_params("INSERT INTO CaracteristicasLocal VALUES ($1,$2,$3,$4,$5)",
                          refInmueble, diafano, numPuertas, aire, supAltillo);
        }

        W.commit();
        cout << "\033[1;32mInmueble registrado exitosamente con referencia " << refInmueble << ".\033[0m\n";
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError al registrar inmueble: " << e.what() << "\033[0m\n";
    }
    pausa();
}

void mostrarInmuebles(connection &c){
    limpiarPantalla();
    try
    {
        nontransaction N(c);
        result R = N.exec(
            "SELECT i.*, o.Nombre as NombreOficina, "
            "CASE WHEN i.Tipo != 'Local' THEN "
            "    (SELECT json_build_object('habitaciones', cv.NumHabitaciones, "
            "                             'baños', cv.NumBanos, "
            "                             'aseos', cv.NumAseos, "
            "                             'cocinas', cv.NumCocinas, "
            "                             'gas_ciudad', cv.TieneGasCiudad, "
            "                             'puerta_blindada', cv.TienePuertaBlindada, "
            "                             'parquet', cv.TieneParquet) "
            "     FROM CaracteristicasVivienda cv WHERE cv.Ref = i.Ref) "
            "ELSE "
            "    (SELECT json_build_object('diafano', cl.Diafano, "
            "                             'puertas', cl.NumPuertas, "
            "                             'aire', cl.TieneAireAcondicionado, "
            "                             'altillo', cl.SuperficieAltillo) "
            "     FROM CaracteristicasLocal cl WHERE cl.Ref = i.Ref) "
            "END as caracteristicas "
            "FROM Inmuebles i "
            "LEFT JOIN Oficinas o ON i.OficinaID = o.ID "
            "ORDER BY i.Ref;");

        for (result::const_iterator row = R.begin(); row != R.end(); ++row)
        {
            cout << "\033[1;36mReferencia: " << row["Ref"].as<int>() << "\033[0m\n";
            cout << "Tipo: " << row["Tipo"].as<string>() << "\n";
            cout << "Oficina: " << row["NombreOficina"].as<string>() << "\n";
            cout << "Superficie: " << row["Superficie"].as<int>() << "m2\n";
            cout << "Ubicación: " << row["Direccion"].as<string>() << " (" << row["Zona"].as<string>() << ")\n";

            float precioVenta = row["PrecioVenta"].is_null() ? 0 : row["PrecioVenta"].as<float>();
            float precioAlquiler = row["PrecioAlquiler"].is_null() ? 0 : row["PrecioAlquiler"].as<float>();

            cout << "Precio Venta: " << formatearPrecio(precioVenta) << "\n";
            cout << "Precio Alquiler: " << (precioAlquiler > 0 ? formatearPrecio(precioAlquiler) + "/mes" : "No disponible") << "\n";
            cout << "Propietario: " << row["Propietario"].as<string>() << "\n";
            cout << "Teléfono: " << row["TelefonoPropietario"].as<string>() << "\n";
            cout << "Llaves: " << (row["TieneLlaves"].as<bool>() ? "Sí" : "No") << "\n";
            cout << "Características: " << row["caracteristicas"].as<string>() << "\n";
            cout << "----------------------------------------\n";
        }
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError al mostrar inmuebles: " << e.what() << "\033[0m\n";
    }
    pausa();
}

void eliminarInmueble(connection &c){
    limpiarPantalla();
    try
    {
        work W(c);
        cout << "\033[1;34m=== ELIMINAR INMUEBLE ===\033[0m\n\n";

        // Obtener lista de inmuebles con información relevante
        result R = W.exec(
            "SELECT i.Ref, i.Tipo, i.Direccion, i.Zona, o.Nombre as NombreOficina "
            "FROM Inmuebles i "
            "LEFT JOIN Oficinas o ON i.OficinaID = o.ID "
            "ORDER BY i.Ref;");

        if (R.empty())
        {
            cout << "\033[1;31mNo hay inmuebles registrados en el sistema.\033[0m\n";
            pausa();
            return;
        }

        // Mostrar lista de inmuebles disponibles
        cout << "Inmuebles disponibles:\n";
        cout << "\033[1;36m";
        cout << setw(8) << left << "REF"
             << setw(15) << left << "TIPO"
             << setw(40) << left << "DIRECCIÓN"
             << setw(30) << left << "ZONA"
             << "OFICINA\n";
        cout << string(100, '-') << "\033[0m\n";

        for (const auto &row : R)
        {
            cout << setw(8) << left << row["Ref"].as<int>()
                 << setw(15) << left << row["Tipo"].as<string>()
                 << setw(40) << left << row["Direccion"].as<string>()
                 << setw(30) << left << row["Zona"].as<string>()
                 << row["NombreOficina"].as<string>() << "\n";
        }
        cout << "\n";

        // Solicitar y validar la referencia del inmueble
        int ref;
        while (true)
        {
            ref = validarEntradaNumerica<int>("Ingrese referencia a eliminar (0 para cancelar): ");

            if (ref == 0)
            {
                cout << "\033[1;33mOperación cancelada por el usuario.\033[0m\n";
                pausa();
                return;
            }

            // Verificar si el inmueble existe
            result checkRef = W.exec_params(
                "SELECT COUNT(*) FROM Inmuebles WHERE Ref = $1",
                ref);

            if (checkRef[0][0].as<int>() > 0)
            {
                break;
            }

            cout << "\033[1;31mError: No existe un inmueble con la referencia "
                 << ref << ".\033[0m\n\n";
        }

        // Mostrar información detallada del inmueble a eliminar
        result infoInmueble = W.exec_params(
            "SELECT i.*, o.Nombre as NombreOficina "
            "FROM Inmuebles i "
            "LEFT JOIN Oficinas o ON i.OficinaID = o.ID "
            "WHERE i.Ref = $1",
            ref);

        cout << "\n\033[1;33mVa a eliminar el siguiente inmueble:\033[0m\n";
        cout << "Referencia: " << infoInmueble[0]["Ref"].as<int>() << "\n"
             << "Tipo: " << infoInmueble[0]["Tipo"].as<string>() << "\n"
             << "Dirección: " << infoInmueble[0]["Direccion"].as<string>() << "\n"
             << "Zona: " << infoInmueble[0]["Zona"].as<string>() << "\n"
             << "Oficina: " << infoInmueble[0]["NombreOficina"].as<string>() << "\n\n";

        // Verificar si hay visitas asociadas
        result visitas = W.exec_params(
            "SELECT COUNT(*) FROM Visitas WHERE RefInmueble = $1",
            ref);

        if (visitas[0][0].as<int>() > 0)
        {
            cout << "\033[1;33mAdvertencia: Este inmueble tiene "
                 << visitas[0][0].as<int>()
                 << " visitas registradas que también serán eliminadas.\033[0m\n\n";
        }

        // Solicitar confirmación
        char confirm;
        do
        {
            cout << "\033[1;31m¿Está seguro de que desea eliminar este inmueble? (s/n): \033[0m";
            cin >> confirm;
            limpiarBuffer();

            confirm = tolower(confirm);
            if (confirm != 's' && confirm != 'n')
            {
                cout << "\033[1;31mPor favor, responda 's' para sí o 'n' para no.\033[0m\n";
            }
        } while (confirm != 's' && confirm != 'n');

        if (confirm == 'n')
        {
            cout << "\033[1;33mOperación cancelada por el usuario.\033[0m\n";
            pausa();
            return;
        }

        // Eliminar el inmueble
        // Nota: Las características y visitas se eliminarán automáticamente por la restricción ON DELETE CASCADE
        W.exec_params("DELETE FROM Inmuebles WHERE Ref = $1", ref);
        W.commit();

        cout << "\033[1;32mInmueble eliminado correctamente.\033[0m\n";
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError al eliminar inmueble: " << e.what() << "\033[0m\n";
    }
    pausa();
}

void buscarInmuebles(connection &c) {
    limpiarPantalla();
    try {
        nontransaction N(c);
        bool menuPrincipal = true;

        while (menuPrincipal) {
            cout << "\033[1;34m=== BÚSQUEDA DE INMUEBLES ===\033[0m\n\n"
                 << "1. Por zona\n"
                 << "2. Por tipo\n"
                 << "3. Por rango de precio\n"
                 << "4. Por oficina\n"
                 << "5. Por características\n"
                 << "0. Volver al menú principal\n\n";

            int opcion = validarEntradaNumerica<int>("Seleccione una opción: ", 0, 5);
            
            if (opcion == 0) {
                return;
            }

            string baseQuery = 
                "SELECT i.*, o.Nombre as Oficina, "
                "CASE WHEN i.Tipo != 'Local' THEN "
                "    (SELECT json_build_object('habitaciones', cv.NumHabitaciones, "
                "                            'baños', cv.NumBanos, "
                "                            'aseos', cv.NumAseos, "
                "                            'cocinas', cv.NumCocinas) "
                "     FROM CaracteristicasVivienda cv WHERE cv.Ref = i.Ref) "
                "ELSE "
                "    (SELECT json_build_object('diafano', cl.Diafano, "
                "                            'puertas', cl.NumPuertas, "
                "                            'aire', cl.TieneAireAcondicionado, "
                "                            'altillo', cl.SuperficieAltillo) "
                "     FROM CaracteristicasLocal cl WHERE cl.Ref = i.Ref) "
                "END as caracteristicas "
                "FROM Inmuebles i "
                "LEFT JOIN Oficinas o ON i.OficinaID = o.ID WHERE ";

            string where;

            switch (opcion) {
                case 1: {  // Búsqueda por zona
                    string zona = validarEntradaTexto("Ingrese la zona a buscar: ", 2, 50);
                    where = "LOWER(i.Zona) LIKE LOWER('%" + N.esc(zona) + "%')";
                    break;
                }
                case 2: {  // Búsqueda por tipo
                    cout << "\nTipos disponibles:\n"
                         << "1. Piso\n"
                         << "2. Casa\n"
                         << "3. Villa\n"
                         << "4. Local\n";
                    
                    int tipoOpcion = validarEntradaNumerica<int>("Seleccione el tipo: ", 1, 4);
                    string tipo;
                    switch (tipoOpcion) {
                        case 1: tipo = "Piso"; break;
                        case 2: tipo = "Casa"; break;
                        case 3: tipo = "Villa"; break;
                        case 4: tipo = "Local"; break;
                    }
                    where = "i.Tipo = " + N.quote(tipo);
                    break;
                }
                case 3: {  // Búsqueda por rango de precio
                    cout << "\n1. Buscar por precio de venta\n"
                         << "2. Buscar por precio de alquiler\n";
                    int tipoPrice = validarEntradaNumerica<int>("Seleccione opción: ", 1, 2);
                    
                    float min = validarEntradaNumerica<float>("Precio mínimo: ", 0, 1000000000);
                    float max = validarEntradaNumerica<float>("Precio máximo: ", min, 1000000000);
                    
                    string campo = (tipoPrice == 1) ? "PrecioVenta" : "PrecioAlquiler";
                    where = "i." + campo + " BETWEEN " + to_string(min) + " AND " + to_string(max);
                    break;
                }
                case 4: {  // Búsqueda por oficina
                    result R = N.exec("SELECT ID, Nombre FROM Oficinas ORDER BY ID");
                    if (R.empty()) {
                        cout << "\033[1;31mNo hay oficinas registradas en el sistema.\033[0m\n";
                        pausa();
                        continue;
                    }

                    cout << "\nOficinas disponibles:\n";
                    for (auto row : R) {
                        cout << row[0].as<int>() << ". " << row[1].as<string>() << "\n";
                    }

                    int id = validarEntradaNumerica<int>("Seleccione oficina: ", 1, R[R.size() - 1][0].as<int>());
                    where = "i.OficinaID = " + to_string(id);
                    break;
                }
                case 5: {  // Búsqueda por características
                    cout << "\n1. Características de vivienda\n"
                         << "2. Características de local\n";
                    int tipoCaract = validarEntradaNumerica<int>("Seleccione opción: ", 1, 2);

                    if (tipoCaract == 1) {
                        cout << "\n1. Número de habitaciones\n"
                             << "2. Número de baños\n"
                             << "3. Tiene gas ciudad\n"
                             << "4. Tiene calefacción\n";
                        
                        int opCaract = validarEntradaNumerica<int>("Seleccione característica: ", 1, 4);
                        
                        switch (opCaract) {
                            case 1: {
                                int hab = validarEntradaNumerica<int>("Mínimo de habitaciones: ", 1, 20);
                                where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                                       "WHERE cv.Ref = i.Ref AND cv.NumHabitaciones >= " + to_string(hab) + ")";
                                break;
                            }
                            case 2: {
                                int ban = validarEntradaNumerica<int>("Mínimo de baños: ", 1, 10);
                                where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                                       "WHERE cv.Ref = i.Ref AND cv.NumBanos >= " + to_string(ban) + ")";
                                break;
                            }
                            case 3:
                                where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                                       "WHERE cv.Ref = i.Ref AND cv.TieneGasCiudad = true)";
                                break;
                            case 4:
                                where = "EXISTS (SELECT 1 FROM CaracteristicasVivienda cv "
                                       "WHERE cv.Ref = i.Ref AND cv.TieneCalefaccion = true)";
                                break;
                        }
                    } else {
                        cout << "\n1. Local diáfano\n"
                             << "2. Número de puertas\n"
                             << "3. Tiene aire acondicionado\n";
                        
                        int opCaract = validarEntradaNumerica<int>("Seleccione característica: ", 1, 3);
                        
                        switch (opCaract) {
                            case 1:
                                where = "EXISTS (SELECT 1 FROM CaracteristicasLocal cl "
                                       "WHERE cl.Ref = i.Ref AND cl.Diafano = true)";
                                break;
                            case 2: {
                                int puertas = validarEntradaNumerica<int>("Mínimo de puertas: ", 1, 10);
                                where = "EXISTS (SELECT 1 FROM CaracteristicasLocal cl "
                                       "WHERE cl.Ref = i.Ref AND cl.NumPuertas >= " + to_string(puertas) + ")";
                                break;
                            }
                            case 3:
                                where = "EXISTS (SELECT 1 FROM CaracteristicasLocal cl "
                                       "WHERE cl.Ref = i.Ref AND cl.TieneAireAcondicionado = true)";
                                break;
                        }
                    }
                    break;
                }
            }

            // Ejecutar la búsqueda
            result R = N.exec(baseQuery + where + " ORDER BY i.Ref");
            limpiarPantalla();
            
            cout << "\033[1;34mResultados encontrados: " << R.size() << "\033[0m\n\n";

            if (R.empty()) {
                cout << "\033[1;33mNo se encontraron inmuebles con los criterios especificados.\033[0m\n";
            } else {
                for (auto row : R) {
                    cout << "\033[1;36mReferencia: " << row["Ref"].as<int>() << "\033[0m\n"
                         << "Tipo: " << row["Tipo"].as<string>() << "\n"
                         << "Superficie: " << row["Superficie"].as<int>() << "m2\n"
                         << "Dirección: " << row["Direccion"].as<string>() << "\n"
                         << "Zona: " << row["Zona"].as<string>() << "\n"
                         << "Oficina: " << row["Oficina"].as<string>() << "\n"
                         << "Precio Venta: " << (row["PrecioVenta"].is_null() ? "No disponible" : formatearPrecio(row["PrecioVenta"].as<float>())) << "\n"
                         << "Precio Alquiler: " << (row["PrecioAlquiler"].is_null() ? "No disponible" : formatearPrecio(row["PrecioAlquiler"].as<float>()) + "/mes") << "\n"
                         << "Características: " << row["caracteristicas"].as<string>() << "\n"
                         << string(50, '-') << "\n";
                }
            }

            cout << "\n¿Desea realizar otra búsqueda?\n";
            menuPrincipal = validarEntradaBooleana("¿Continuar?");
            limpiarPantalla();
        }
    }
    catch (const exception &e) {
        cerr << "\033[1;31mError en la búsqueda: " << e.what() << "\033[0m\n";
    }
    pausa();
}

void menu(connection &c){
    int opcion;
    do
    {
        limpiarPantalla();
        cout << "\033[1;34m=== SISTEMA INMOBILIARIO ===\033[0m\n\n"
             << "1. Registrar inmueble\n"
             << "2. Mostrar inmuebles\n"
             << "3. Buscar inmuebles\n"
             << "4. Eliminar inmueble\n"
             << "0. Salir\n\n"
             << "Opción: ";

        if (!(cin >> opcion))
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            opcion = -1;
        }
        limpiarBuffer();

        try
        {
            switch (opcion)
            {
            case 1:
                agregarInmueble(c);
                break;
            case 2:
                mostrarInmuebles(c);
                break;
            case 3:
                buscarInmuebles(c);
                break;
            case 4:
                eliminarInmueble(c);
                break;
            case 0:
                limpiarPantalla();
                cout << "Finalizando...\n";
                system("sleep 1");
                break;
            default:
                cout << "\033[1;31mOpción no válida\033[0m\n";
                pausa();
            }
        }
        catch (const exception &e)
        {
            cerr << "\033[1;31mError: " << e.what() << "\033[0m\n";
            pausa();
        }
    } while (opcion != 0);
}

int main(){
    limpiarPantalla();
    cout << "\033[1;34m=== SISTEMA INMOBILIARIO ===\033[0m\n\n";

    try
    {
        connection c = conectar();
        system("sleep 1");

        cout << "Inicializando...\n";
        crearTablasInmobiliaria(c);
        system("sleep 1");

        menu(c);
    }
    catch (const exception &e)
    {
        cerr << "\033[1;31mError fatal: " << e.what() << "\033[0m\n";
        pausa();
        return 1;
    }

    return 0;
}