import sqlite3
from flask import Flask, jsonify, request, render_template, redirect, url_for, flash
import json
import socket
import time

app = Flask(__name__)

app.secret_key = 'mi_clave_super_secreta'

DB_NAME = "usuarios_cerradura.db"

# Variable para saber cuándo fue el último cambio
LAST_UPDATE_TIME = time.time()

def notificar_cambio():
    global LAST_UPDATE_TIME
    LAST_UPDATE_TIME = time.time()

# --- CONFIGURACIÓN DE BASE DE DATOS ---
def init_db():
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS usuarios (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            codigo TEXT NOT NULL,
            activo INTEGER DEFAULT 1,
            rfid TEXT DEFAULT "",
            huella TEXT DEFAULT "",
            tag TEXT DEFAULT ""
        )
    ''')
    
    # Crear ADMIN si está vacía (Asumimos que siempre será ID 1)
    cursor.execute('SELECT count(*) FROM usuarios')
    if cursor.fetchone()[0] == 0:
        cursor.execute('''
            INSERT INTO usuarios (codigo, activo, rfid, huella, tag)
            VALUES (?, ?, ?, ?, ?)
        ''', ("11111", 1, "", "", "ADMIN"))
        print("Base de datos inicializada con usuario ADMIN")
        conn.commit()
    conn.close()

def get_db_connection():
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row 
    return conn

# --- RUTA 1: ELIMINAR USUARIO ---
@app.route('/delete/<int:user_id>', methods=['POST'])
def delete_user(user_id):
    # Protección: No dejar borrar al ID 1 (Master)
    if user_id == 1:
        return "ERROR: No se puede borrar al usuario Maestro (ID 1)", 403

    conn = get_db_connection()
    conn.execute('DELETE FROM usuarios WHERE id = ?', (user_id,))
    conn.commit()
    conn.close()
    notificar_cambio();
    print(f"Usuario ID {user_id} eliminado desde la web.")
    return redirect(url_for('panel_admin'))

# --- RUTA 2: API PARA EL ESP32 (JSON) ---
@app.route('/syncPins/', methods=['GET'])
def sync_pins():
    conn = get_db_connection()
    usuarios_db = conn.execute('SELECT * FROM usuarios WHERE activo = 1').fetchall()
    conn.close()

    lista_usuarios = []
    for row in usuarios_db:
        usuario = {
            "codigo": row['codigo'],
            "activo": bool(row['activo']), 
            "rfid": row['rfid'] if row['rfid'] else "",
            "huella": row['huella'] if row['huella'] else "",
            "tag": row['tag'] if row['tag'] else "usuario"
        }
        lista_usuarios.append(usuario)

    print(f"ESP32 solicitó sync. Enviando {len(lista_usuarios)} usuarios.")
    # separators=(',', ':') elimina los espacios en blanco
    json_compacto = json.dumps({"pins": lista_usuarios}, separators=(',', ':'))
    
    return app.response_class(
        response=json_compacto,
        mimetype='application/json'
    )
    
# --- RUTA 3: ELIMINAR RFID ---
@app.route('/delete_rfid/<int:user_id>', methods=['POST'])
def delete_rfid(user_id):
    conn = get_db_connection()
    
    # Verificamos si el usuario existe (opcional, pero buena práctica)
    user = conn.execute('SELECT * FROM usuarios WHERE id = ?', (user_id,)).fetchone()
    
    if user:
        # Borramos solo el RFID (lo dejamos en blanco), NO borramos al usuario
        conn.execute('UPDATE usuarios SET rfid = "" WHERE id = ?', (user_id,))
        conn.commit()
        
        # Avisamos al sistema que hubo cambios
        notificar_cambio()
        
        flash(f'RFID desvinculado del usuario {user["tag"]}.', 'warning')
    else:
        flash('Usuario no encontrado.', 'error')
    
    conn.close()
    return redirect(url_for('panel_admin')) # O 'index' según como llames a tu función principal

@app.route('/update_admin_pin', methods=['POST'])
def update_admin_pin():
    try:
        nuevo_pin = request.form['pin_admin']
        
        # Validar que sean 5 dígitos
        if len(nuevo_pin) != 5 or not nuevo_pin.isdigit():
             flash('Error: El PIN Admin debe tener 5 números.', 'danger')
             return redirect(url_for('panel_admin'))

        conn = get_db_connection()
        
        # Actualizamos SOLO al usuario ID 1
        conn.execute('UPDATE usuarios SET codigo = ? WHERE id = 1', (nuevo_pin,))
        conn.commit()
        notificar_cambio()
        conn.close()

        flash('¡Clave de Administrador actualizada con éxito!', 'success')
        
    except Exception as e:
        flash(f'Error al actualizar: {e}', 'danger')

    return redirect(url_for('panel_admin'))         

@app.route('/update-rfid', methods=['GET'])
def update_rfid_endpoint():
    pin = request.args.get('pin')
    rfid = request.args.get('rfid')
    if not pin or not rfid: return "ERROR: Faltan parametros", 400

    conn = get_db_connection()
    user = conn.execute('SELECT * FROM usuarios WHERE codigo = ?', (pin,)).fetchone()
    if user:
        conn.execute('UPDATE usuarios SET rfid = ? WHERE codigo = ?', (rfid, pin))
        conn.commit()
        conn.close()
        notificar_cambio()
        return "UPDATED OK"
    else:
        conn.close()
        return "ERROR: PIN NO EXISTE", 404

# Borrar huella de un usuario específico
@app.route('/delete-huella', methods=['GET'])
def delete_huella_endpoint():
    pin = request.args.get('pin')
    if not pin: return "ERROR: Falta PIN", 400

    conn = get_db_connection()
    # Ponemos el campo huella en blanco
    conn.execute('UPDATE usuarios SET huella = "" WHERE codigo = ?', (pin,))
    conn.commit()
    conn.close()
    notificar_cambio()
    print(f"[API] Huella eliminada del PIN {pin}")
    return "DELETED OK"

# Borrar TODAS las huellas (Reseteo de fábrica de huellas)
@app.route('/clear-all-huellas', methods=['GET'])
def clear_all_huellas_endpoint():
    conn = get_db_connection()
    # Ponemos todas las huellas en blanco
    conn.execute('UPDATE usuarios SET huella = ""')
    conn.commit()
    conn.close()
    notificar_cambio()
    print(f"[API] TODAS las huellas han sido eliminadas.")
    return "ALL CLEARED OK"

@app.route('/update-huella', methods=['GET'])
def update_huella_endpoint():
    pin = request.args.get('pin')
    huella_id = request.args.get('huella')
    
    if not pin or not huella_id:
        return "ERROR: Faltan parametros", 400

    conn = get_db_connection()
    user = conn.execute('SELECT * FROM usuarios WHERE codigo = ?', (pin,)).fetchone()
    
    if user:
        conn.execute('UPDATE usuarios SET huella = ? WHERE codigo = ?', (huella_id, pin))
        conn.commit()
        conn.close()
        notificar_cambio()
        print(f"[API] Huella ID {huella_id} asignada al PIN {pin}.")
        return "UPDATED OK"
    else:
        conn.close()
        return "ERROR: PIN NO EXISTE", 404

@app.route('/check-status')
def check_status():
    return jsonify({"last_update": LAST_UPDATE_TIME})
  
# --- FUNCIÓN AUXILIAR PARA SABER LA IP REAL DEL PC ---
def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # No se conecta realmente, solo consulta qué IP usaría para salir a internet
        s.connect(('8.8.8.8', 1)) 
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP 
            
# --- RUTA 3: PANEL WEB ---
@app.route('/', methods=['GET', 'POST'])
def panel_admin():
    conn = get_db_connection()
    
    if request.method == 'POST':
        # Agregar usuario
        if 'codigo' in request.form:
            codigo = request.form['codigo']
            tag = request.form['tag']
            rfid = request.form['rfid']
            huella = request.form['huella']
            
            # --- 1. VALIDACIÓN DE DUPLICADOS ---
            # Buscamos si ya existe ese código en la tabla
            user = conn.execute('SELECT * FROM usuarios WHERE codigo = ?', (codigo,)).fetchone()
            
            if user:
                conn.close()
                flash(f'¡Error! El PIN {codigo} ya existe.', 'danger')
                return redirect(url_for('panel_admin'))
            
            conn.execute('INSERT INTO usuarios (codigo, tag, rfid, huella) VALUES (?, ?, ?, ?)',
                         (codigo, tag, rfid, huella))
            conn.commit()
            notificar_cambio()
            conn.close()
            flash('Usuario agregado correctamente.', 'success')
            return redirect(url_for('panel_admin'))
            
    rows = conn.execute('SELECT * FROM usuarios').fetchall()
    conn.close()
    usuarios = [dict(row) for row in rows]
    return render_template('index.html', usuarios=usuarios, server_time=LAST_UPDATE_TIME)

if __name__ == '__main__':
    init_db()
    try:
        ip_actual = get_local_ip()
        print(f"Servidor HTTP CORRIENDO en: {ip_actual}:8000")
        app.run(host='0.0.0.0', port=8000, debug=True, threaded=True)
    except Exception as e:
        print(f"Error fatal al iniciar: {e}")
        time.sleep(10) # Pausa para leer el error si se cierra
 