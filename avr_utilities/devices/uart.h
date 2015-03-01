//
//  Copyright (C) 2015 Danny Havenith
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef AVR_UTILITIES_DEVICES_UART_H_
#define AVR_UTILITIES_DEVICES_UART_H_
#include "avr_utilities/round_robin_buffer.h"

#include <avr/io.h>
#include <avr/interrupt.h>

/**
 * Use this macro to enable UART interrupt handling by
 * a specific UART object.
 */
#define IMPLEMENT_UART_INTERRUPT( uart_)            \
    ISR( USART_UDRE_vect)                           \
    {                                               \
        uart_.uart_empty_interrupt();               \
    }                                               \
    /**/

namespace serial
{
    /**
     * This class eases using the AVRs UART. It provides buffered output (input to be implemented later).
     *
     */
    template< uint8_t output_buffer_size = 32>
    class uart
    {
    public:
        uart( uint32_t baudrate)
            :idle(true)
        {
            init();
            set_baudrate( baudrate);
        }

        static void set_baudrate( uint32_t baudrate)
        {
            const unsigned long timerVal
                = ((F_CPU + 8UL * baudrate)/(16UL * baudrate)) - 1;

            UBRR0L = (uint8_t)timerVal;
            UBRR0H = (uint8_t)timerVal >> 8;
        }

        static void init()
        {
            // enable TX
            // enable UDR-empty interrupt
            UCSR0B = (1<<TXEN0) | (1 << UDRIE0);

            // 8-bits data, no parity, 1 stopbit (8n1)
            UCSR0C = _BV( UCSZ01) | _BV( UCSZ00);
        }
        /**
         * This function must be called by the application program whenever the UART data register is empty.
         * Normally, this is done from the UDRE-interrupt, for example as follows:
         *
         * \code
         *  /// Data register empty interrupt
         *  ISR( USART_UDRE_vect)
         *  {
         *      my_transmitter.uart_empty_interrupt();
         *  }
         * \endcode
         *
         *
         */
        void uart_empty_interrupt() volatile
        {
            uint8_t byte = 0;

            // try to read the next character to send
            if (output_buffer.read( &byte))
            {
                // ... OK, send it.
                UDR0 = byte;
            }
            else
            {
                // disable interrupt, we're idle
                idle = true;
                UCSR0B &= ~(1 << UDRIE0);
            }
        }

        void send( const char *message) volatile
        {
            while (*message)
            {
                append( static_cast<uint8_t>(*message));
                ++message;
            }
            commit();
        }

    private:
        /// commit all appends since the previous commit.
        /// this will actually send the appended bytes to output.
        void commit() volatile
        {
            output_buffer.commit();
            cli();
            if (idle)
            {
                // re-enable interrupt
                UCSR0B |= (1 << UDRIE0);
                UDR0 = output_buffer.read_w(); // start the UART and its interrupts
            }
            sei();
        }

        /// cancel all appends since the last commit
        void abort() volatile
        {
            output_buffer.reset_tentative();
        }

        /**
         * Offer a byte for tentative write.
         *
         * Note that nothing is transmitted until commit is called.
         * Typically an application will add one logical 'packet' of byte data using this function and the call commit.
         */
        bool append( uint8_t byte) volatile
        {
            return output_buffer.write_tentative( byte);
        }

        /**
         * Offer a 16-bit word for tentative write. This word will be added in big-endian format to the buffer for transmit.
         *
         * Note that nothing is transmitted until commit is called.
         * Typically an application will add one logical 'packet' of data using this function and the call commit.
         */
        bool append( uint16_t word) volatile
        {
            bool result = output_buffer.write_tentative( word >> 8);
            return result && output_buffer.write_tentative( word);
        }


        bool idle;
        round_robin_buffer<output_buffer_size> output_buffer;

    };
}
#endif /* AVR_UTILITIES_DEVICES_UART_H_ */