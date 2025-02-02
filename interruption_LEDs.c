// Bibliotecas padrão e de hardware
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"

#define IS_RGBW false // Definir falso se os LEDs são RGB
#define NUM_PIXELS 25 // Número de LEDs na matriz
#define WS2812_PIN 7 // Pino usado para controlar os LEDs WS2812
#define PIN_LED_R 13 // Pino do LED vermelho
#define BUTTON_A 5 // Pino do botão A
#define BUTTON_B 6 // Pino do botão B

// Variáveis globais para armazenar a cor dos LEDs na matriz (entre 0 e 255 para intensidade)
uint8_t led_r = 0;
uint8_t led_g = 0;
uint8_t led_b = 255;

// Variáveis globais para controle do incremento/decremento e tempo do último evento
static volatile uint8_t incrementar_decrementar = 0;
static volatile uint32_t last_time = 0;

// Matriz para armazenar os números (0 a 9) que serão exibidos
bool numeros[10][NUM_PIXELS] = {
    {
    0, 0, 1, 0, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 1, 1, 1, 0, 
    0, 0, 1, 0, 0, 
    0, 0, 1, 0, 0, 
    0, 1, 1, 0, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 1, 1, 1, 0, 
    0, 1, 0, 0, 0, 
    0, 0, 1, 0, 0, 
    0, 1, 0, 1, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 0, 1, 1, 0, 
    0, 0, 0, 1, 0, 
    0, 1, 0, 0, 0, 
    0, 0, 1, 0, 0, 
    0, 1, 1, 1, 0
    },
    {
    0, 1, 1, 1, 0, 
    0, 0, 1, 0, 0, 
    0, 1, 1, 1, 1, 
    0, 1, 1, 0, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 0, 1, 1, 0, 
    0, 0, 0, 1, 0, 
    0, 1, 0, 0, 0, 
    0, 1, 1, 0, 0, 
    0, 1, 1, 1, 0
    },
    {
    0, 0, 1, 0, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 1, 1, 0, 
    0, 1, 0, 0, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 0, 0, 1, 0, 
    0, 0, 1, 0, 0, 
    0, 0, 0, 0, 0, 
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0
    },
    {
    0, 0, 1, 0, 0, 
    0, 1, 0, 1, 0, 
    0, 0, 1, 0, 0, 
    0, 1, 0, 1, 0, 
    0, 0, 1, 0, 0
    },
    {
    0, 0, 1, 0, 0, 
    0, 0, 0, 1, 0, 
    0, 1, 1, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 0, 1, 0, 0
    }
};

// Prototipação das rotinas
static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void set_one_led(uint8_t r, uint8_t g, uint8_t b, uint8_t numero);
static void gpio_irq_handler(uint gpio, uint32_t events);
bool repeating_timer_callback(struct repeating_timer *t);


// Rotina principal
int main()
{   
    // Inicializa o pino do LED e define a direção como saída
    gpio_init(PIN_LED_R);
    gpio_set_dir(PIN_LED_R, GPIO_OUT);
    
    // Inicializa o pino do botão A e define a direção como entrada, com pull-up
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    // Inicializa o pino do botão B e define a direção como entrada, com pull-up
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    
    // Inicializa o PIO e configura o estado da máquina e o programa WS2812
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    // Configura o programa WS2812
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Configuração da interrupção com callback para os botões A e B
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Declaração de uma estrutura de temporizador de repetição
    // Esta estrutura armazenará informações sobre o temporizador configurado
    struct repeating_timer timer;

    // Configura o temporizador para chamar a função de callback a cada 100 ms
    add_repeating_timer_ms(100, repeating_timer_callback, NULL, &timer);

    // Exibe o número zero inicialmente
    set_one_led(led_r, led_g, led_b, incrementar_decrementar);

    while (1)
    {
        // Laço principal 
    }

    return 0;
}


// Envia o valor do pixel para a máquina de estado do PIO
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Converte os valores RGB para um único valor de 32 bits
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Define a cor de um LED específico na matriz
void set_one_led(uint8_t r, uint8_t g, uint8_t b, uint8_t numero)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // // Percorre todos os LEDs e define a cor com base no valor da matriz numeros
    for (uint8_t i = 0; i < NUM_PIXELS; i++)
    {
        if (numeros[numero][i])
        {
            put_pixel(color);
        }
        else
        {
            put_pixel(0);
        }
    }
}

// Função de interrupção com debouncing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) // 200 ms de debouncing
    {
        last_time = current_time; // Atualiza o tempo do último evento

        // // Incrementa ou decrementa o valor com base no botão pressionado
        if (gpio == 5)
        {
            if (incrementar_decrementar < 9)
                incrementar_decrementar++;
        }
        else if (gpio == 6) 
        {
            if (incrementar_decrementar > 0)
                incrementar_decrementar--;
        }

        // Exibe o número atual após incrementar/decrementar
        set_one_led(led_r, led_g, led_b, incrementar_decrementar);
    }
}

// Função de callback que será chamada repetidamente pelo temporizador
// O tipo bool indica que a função deve retornar verdadeiro ou falso para continuar ou parar o temporizador.
bool repeating_timer_callback(struct repeating_timer *t)
{
    // Liga ou desliga o LED no pino 13
    gpio_put(PIN_LED_R, !gpio_get(PIN_LED_R));
    
    // Retorna true para manter o temporizador repetindo. Se retornar false, o temporizador para
    return true;
}