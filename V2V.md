Linkedin Articel: 'https://www.linkedin.com/pulse/esp-now-how-esp32-devices-communicate-without-wi-fi-manjunath-patil-wwlcf/'

Wireless communication is an important part of many embedded and IoT systems. In a typical Wi-Fi-based system, devices communicate through a network such as a router or access point. However, some applications require devices to exchange small amounts of data directly, with less communication overhead.

This is where ESP-NOW becomes useful.

ESP-NOW is a connectionless wireless communication protocol developed by Espressif for ESP32 and other supported ESP devices. It allows ESP devices to exchange data directly without requiring a conventional Wi-Fi access point.

In my work with ESP32-based systems, I explored ESP-NOW as a method for direct communication between devices. This also made me interested in its potential for applications such as vehicle-to-vehicle (V2V) communication, where one device may need to share information such as speed, position or warning status with another device.

In this article, I will explain how ESP-NOW works, how data is exchanged between ESP32 devices, where it can be useful and some of its practical limitations.

1. What is ESP-NOW?
ESP-NOW is a wireless communication protocol designed by Espressif that allows supported ESP devices to communicate directly with each other.

Unlike a conventional Wi-Fi setup, the communicating devices do not necessarily need a router or access point to exchange data.

A simplified communication model looks like this:

ESP32 A → ESP-NOW → ESP32 B

The devices can exchange relatively small data packets directly over the wireless link.

This makes ESP-NOW useful for applications where an embedded system needs to periodically transmit sensor readings, control information or status data to another ESP device.


See content credentials
Article content
2. How is ESP-NOW different from normal Wi-Fi?
One of the easiest ways to understand ESP-NOW is to compare it with a conventional Wi-Fi communication setup.

In a traditional Wi-Fi system, the communication may look like:

ESP32 → Wi-Fi Router → ESP32

The router acts as part of the network infrastructure.

With ESP-NOW, the communication can instead be:

ESP32 → ESP-NOW → ESP32

The devices can communicate directly without setting up a conventional Wi-Fi network through an access point.

This does not mean that ESP-NOW is simply "faster Wi-Fi." They are better understood as different communication approaches suited to different requirements.

For embedded applications involving small packets and direct device-to-device communication, ESP-NOW can provide a convenient approach.

3. How does ESP-NOW communication work?
Before two ESP32 devices exchange application data, the devices need to be configured appropriately for ESP-NOW communication.

A simplified process is:

Initialize the wireless interface.
Initialize ESP-NOW.
Configure the peer device.
Prepare the data to be transmitted.
Send the data.
Handle the transmission result.
Receive and process the data on the other ESP32.

The exact implementation depends on the ESP-IDF or Arduino environment being used and the ESP device/SDK version.

At the application level, the important concept is that the embedded program prepares a data structure and transmits it to another device.


See content credentials
Article content


4. Sending structured data
One useful feature when developing embedded applications is the ability to organize multiple values into a structure.

For example:

typedef struct {
    float speed;
    float temperature;
    int vehicle_id;
} Data; 
An application can create an instance of this structure:

Data myData;

myData.speed = 45.5;
myData.temperature = 28.4;
myData.vehicle_id = 101; 
The data can then be serialized/transmitted according to the application's ESP-NOW implementation.

At the receiving side, the received packet can be interpreted and the individual fields can be used by the application.

This is particularly useful in embedded systems because multiple related parameters can be organized into a single application-level message.

5. ESP-NOW in an embedded system
Consider a simple system where one ESP32 collects information from sensors and another ESP32 needs to receive it.

The first ESP32 could act as a transmitting node:

Sensors → ESP32 → ESP-NOW

The second ESP32 could act as a receiving node:

ESP-NOW → ESP32 → Application

The receiving application could then display the information, store it, process it or use it as an input to another algorithm.

This architecture can be useful for applications such as:

Sensor networks
Remote monitoring
Home automation
Robotics
Distributed embedded systems
Device-to-device control
Prototype vehicle communication systems

The actual suitability depends on requirements such as range, packet size, reliability, network size, interference and security.

6. Why did I find ESP-NOW interesting for V2V communication?
One application that particularly interested me was vehicle-to-vehicle communication.

Imagine two vehicles approaching a blind curve.

A vehicle on one side may not be directly visible to a vehicle approaching from the opposite direction.

If the vehicles can exchange relevant information, one vehicle could potentially inform the other about its presence or state.

A simplified concept could look like:

Vehicle A

GPS + Speed + Heading

↓

ESP32

↓

ESP-NOW

↓

ESP32

↓

Vehicle B

The receiving vehicle could then use the information as an input to a collision-risk or warning algorithm.

For example, the application could consider parameters such as:

Relative position
Vehicle speed
Direction of travel
Distance
Time-to-collision
Warning status

It is important to distinguish this prototype communication concept from production automotive V2X systems. Automotive deployments involve much more stringent requirements for reliability, latency, security, interoperability, spectrum usage and safety.


See content credentials
Article content
7. ESP-NOW is not the same as automotive V2X
This distinction is important.

ESP-NOW can be very useful for prototyping direct wireless communication between ESP-based embedded systems, but it should not automatically be treated as an automotive V2X standard.

Automotive communication technologies such as C-V2X and IEEE 802.11p-based systems are designed around different communication, interoperability, spectrum and deployment requirements.

Therefore, in a prototype, ESP-NOW can be used to investigate concepts such as:

Vehicle state → Wireless message → Receiving vehicle → Risk analysis → Warning

The communication technology used in a prototype and the communication technology used in a production automotive system do not have to be the same.

This distinction is important when designing research prototypes.

8. What happens when communication is unreliable?
Wireless communication is never something that should simply be assumed to be perfect.

A practical embedded system needs to consider situations such as:

Packet loss
Interference
Communication range limitations
Incorrect or outdated data
Device failure
Multiple devices communicating simultaneously
Security considerations

For a safety-related application, the receiving system should therefore not blindly trust a single received packet.

Instead, the application can incorporate mechanisms such as:

Message → Validation → Timestamp/Sequence Check → Processing → Decision

For example, if a vehicle receives an old position message, using that information directly could lead to an incorrect decision.

This is where communication design and application-level algorithms become equally important.

9. What I learned from working with ESP-NOW
Working with ESP-NOW helped me understand that wireless communication in embedded systems is more than simply sending data from one device to another.

There are several layers to consider:

Hardware

ESP32, sensors, power supply and peripherals.

↓

Communication

ESP-NOW and wireless data exchange.

↓

Data

How information is structured and transmitted.

↓

Processing

How the receiver interprets the received information.

↓

Application

What action should be taken based on that information.

This way of thinking is particularly important in embedded systems, where hardware, software and communication protocols have to work together.

10. Limitations and practical considerations
ESP-NOW is useful, but it is not a universal solution.

Some important considerations include:

Range
The practical communication range depends on the hardware, antenna, environment, interference and configuration.

Reliability
Wireless packets can be lost or delayed. Applications should therefore consider how communication failures are handled.

Scalability
A system with two devices is much simpler than a system involving many communicating nodes. Network design becomes more important as the number of devices increases.

Security
If the exchanged information is important, authentication and encryption mechanisms need to be considered appropriately.

Application requirements
A prototype that works well in a controlled environment does not automatically satisfy the requirements of a real-world safety-critical system.

These factors should be evaluated before selecting a communication technology for a particular application.

Conclusion
ESP-NOW provides an interesting way to explore direct wireless communication between ESP-based embedded systems.

What initially looks like a simple concept — sending data from one ESP32 to another — involves several important embedded-system concepts: device configuration, data structures, wireless communication, packet handling, reliability, security and application-level processing.

For me, exploring ESP-NOW also provided a useful connection between embedded systems and V2V communication.

A simple ESP32-to-ESP32 communication link can serve as a prototype for understanding the larger concept of exchanging vehicle-state information between nodes.

The important lesson is not just how to send a packet.

It is understanding what information should be communicated, how reliably it can be communicated, how the receiving system should interpret it and what decision should be made from that information.

That is what makes communication an interesting part of embedded-system design.
