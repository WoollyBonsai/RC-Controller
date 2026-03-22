package com.example.rccontroller

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress

class MainActivity : ComponentActivity() {
    private val scope = CoroutineScope(Dispatchers.IO + Job())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            RCControllerApp { sendSignal(it) }
        }
    }

    private fun sendSignal(signal: Byte) {
        scope.launch {
            try {
                val address = InetAddress.getByName("192.168.4.1")
                val port = 4210
                val buf = byteArrayOf(signal)
                val socket = DatagramSocket()
                val packet = DatagramPacket(buf, buf.size, address, port)
                socket.send(packet)
                socket.close()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
    }
}

@Composable
fun RCControllerApp(onSend: (Byte) -> Unit) {
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center
            ) {
                Button(onClick = { onSend(1) }) { Text("Accele") }
                Spacer(modifier = Modifier.height(16.dp))
                Button(onClick = { onSend(2) }) { Text("Brake") }
            }
            Row(
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Button(onClick = { onSend(3) }) { Text("Left") }
                Spacer(modifier = Modifier.width(16.dp))
                Button(onClick = { onSend(4) }) { Text("Right") }
            }
        }
    }
}
