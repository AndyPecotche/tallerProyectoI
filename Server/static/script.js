function cerrarAlerta(boton) {
	var alerta = boton.parentElement;
	alerta.style.display = 'none';
}

// Función que pregunta cada 5 segundos si algo cambió
setInterval(function() {
	fetch('/check-status').then(response => response.json()).then(data => {
		// Si la fecha del servidor es más nueva que la nuestra -> RECARGAR
		if (data.last_update > localVersion) {
			console.log("Cambio detectado. Actualizando...");
			location.reload(); 
		}
	}).catch(err => console.error("Error conectando al servidor", err));
}, 5000); // 5000 ms = 5 segundos
