# Development Notes

## Serial Command Limitations

### Blocking Animations
The current implementation of `winAnimation2()` and `loseAnimation()` uses the `delay()` function to control timing. This is a **blocking** operation that stops the CPU from executing the main `loop()`.

**Consequence:**
While an animation is playing, `handleSerial()` is not being called. Any characters sent over the Serial line during this time (approx. 1.3 seconds for a win) will sit in the hardware Serial buffer (typically 64 bytes). 

If multiple commands or long strings are sent during an animation, the buffer may overflow, leading to "eaten" or corrupted messages.

### Future Improvements (Nice-to-Have)
To make Serial triggers 100% responsive even during animations:
1.  **Replace `delay()`**: Move to a non-blocking state machine for animations using `millis()`.
2.  **Service Serial during delays**: Create a helper like `delayAndServiceSerial(ms)` that calls `handleSerial()` while waiting.
3.  **Hardware Interrupts**: Use Serial interrupts to buffer incoming data, though this is more complex.

Current workaround: Wait for the animation to finish (LEDs returning to the goal/idle state) before sending the next `score` or `fail` command.
