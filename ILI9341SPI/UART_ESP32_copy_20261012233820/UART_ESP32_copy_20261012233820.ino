
#include <Bluepad32.h>

// =====================================================
// GALAGA - CONTROL DUALSENSE PS5
// ESP32 Bluetooth -> UART -> STM32F446RE
//
// ESP32 GPIO17 -> STM32 PA10 (USART1_RX)
// ESP32 GND    -> STM32 GND
//
// UART: 115200 baudios, 8N1
// =====================================================

// Pines UART2 del ESP32
#define RXD2 16
#define TXD2 17

// Protocolo UART utilizado por Galaga
#define CONTROL_INICIO 0xA5
#define CONTROL_FIN    0x5A

// Mascara de botones
#define CONTROL_IZQ    (1U << 0)
#define CONTROL_DER    (1U << 1)
#define CONTROL_ACCION (1U << 2)
#define CONTROL_ATRAS  (1U << 3)
#define CONTROL_ARRIBA (1U << 4)
#define CONTROL_ABAJO  (1U << 5)

// Configuracion
#define ENVIO_MS 20
#define ZONA_MUERTA 160

ControllerPtr dualsense = nullptr;

uint32_t ultimoEnvio = 0;
uint8_t estadoAnterior = 0xFF;

// =====================================================
// ENVIO UART A STM32
// =====================================================

void enviarControl(uint8_t botones)
{
    uint8_t paquete[4];

    paquete[0] = CONTROL_INICIO;
    paquete[1] = botones;
    paquete[2] = (uint8_t)(~botones);
    paquete[3] = CONTROL_FIN;

    Serial2.write(paquete, sizeof(paquete));
}

// =====================================================
// CONEXION BLUETOOTH
// =====================================================

void onConnectedController(ControllerPtr ctl)
{
    if (ctl->isGamepad() && dualsense == nullptr)
    {
        dualsense = ctl;

        Serial.println();
        Serial.println("DUALSENSE CONECTADO");
        Serial.println("Control listo para GALAGA");

        // LED azul del DualSense
        ctl->setColorLED(0, 100, 255);
    }
}

void onDisconnectedController(ControllerPtr ctl)
{
    if (dualsense == ctl)
    {
        dualsense = nullptr;

        Serial.println();
        Serial.println("DUALSENSE DESCONECTADO");

        // Soltar todos los botones inmediatamente
        enviarControl(0x00);
        estadoAnterior = 0xFF;
    }
}

// =====================================================
// LECTURA DEL DUALSENSE
// =====================================================

uint8_t leerDualSense()
{
    if (dualsense == nullptr)
        return 0x00;

    if (!dualsense->isConnected() ||
        !dualsense->isGamepad())
        return 0x00;

    uint8_t botones = 0;

    // Joystick izquierdo
    int32_t ejeX = dualsense->axisX();
    int32_t ejeY = dualsense->axisY();

    // Cruceta
    uint8_t cruceta = dualsense->dpad();

    bool izquierda =
        (ejeX < -ZONA_MUERTA) ||
        ((cruceta & DPAD_LEFT) != 0);

    bool derecha =
        (ejeX > ZONA_MUERTA) ||
        ((cruceta & DPAD_RIGHT) != 0);

    // Evitar direcciones horizontales simultaneas
    if (izquierda && !derecha)
    {
        botones |= CONTROL_IZQ;
    }

    if (derecha && !izquierda)
    {
        botones |= CONTROL_DER;
    }

    // Boton X (cruz) del DualSense
    // Bluepad32 lo identifica como A
    if (dualsense->a())
    {
        botones |= CONTROL_ACCION;
    }

    // Circulo del DualSense
    if (dualsense->b())
    {
        botones |= CONTROL_ATRAS;
    }

    // Arriba y abajo para futuras funciones
    if ((ejeY < -ZONA_MUERTA) ||
        ((cruceta & DPAD_UP) != 0))
    {
        botones |= CONTROL_ARRIBA;
    }

    if ((ejeY > ZONA_MUERTA) ||
        ((cruceta & DPAD_DOWN) != 0))
    {
        botones |= CONTROL_ABAJO;
    }

    return botones;
}

// =====================================================
// CONFIGURACION INICIAL
// =====================================================

void setup()
{
    // Monitor serial USB
    Serial.begin(115200);

    // UART2 hacia STM32 USART1_RX (PA10)
    Serial2.begin(
        115200,
        SERIAL_8N1,
        RXD2,
        TXD2
    );

    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println("       GALAGA - DUALSENSE");
    Serial.println("================================");
    Serial.println("Bluetooth: Bluepad32");
    Serial.println("UART TX: GPIO17");
    Serial.println("STM32 RX: PA10");
    Serial.println("Baudrate: 115200");
    Serial.println();

    // Inicializar Bluepad32
    BP32.setup(
        &onConnectedController,
        &onDisconnectedController
    );

    // Solo controles fisicos
    BP32.enableVirtualDevice(false);

    // Descomentar SOLO si necesitas borrar
    // emparejamientos Bluetooth anteriores.
    // BP32.forgetBluetoothKeys();

    enviarControl(0x00);

    Serial.println("Esperando DualSense PS5...");
    Serial.println("Presiona CREATE + PS");
    Serial.println();
}

// =====================================================
// BUCLE PRINCIPAL
// =====================================================

void loop()
{
    // Actualizar eventos Bluetooth
    BP32.update();

    uint32_t ahora = millis();

    // Transmision periodica cada 20 ms
    if ((uint32_t)(ahora - ultimoEnvio) >= ENVIO_MS)
    {
        ultimoEnvio = ahora;

        uint8_t botones = leerDualSense();

        // Enviar siempre, incluso cuando no
        // haya botones presionados.
        enviarControl(botones);

        // Diagnostico por monitor serial
        // Mostrar solo cuando cambia el estado.
        if (botones != estadoAnterior)
        {
            estadoAnterior = botones;

            Serial.printf(
                "BOTONES: 0x%02X | IZQ:%d DER:%d X:%d\n",
                botones,
                (botones & CONTROL_IZQ) != 0,
                (botones & CONTROL_DER) != 0,
                (botones & CONTROL_ACCION) != 0
            );
        }
    }

    delay(1);
}
