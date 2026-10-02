# Game Design: LED Game

## Objective
Accumulate the highest score possible before reaching the maximum number of deaths.

## Scoring Rules
*   **Point Awarded:** Every successful catch (hitting the yellow goal area) increases the `score` by **+1**.
*   **Persistence:** Scoring a point **does not** reset or clear your current death count. Deaths are cumulative over the entire run.

## Death & Game Over
*   **Failure:** Missing a catch or triggering early/late increments the `failCount` (Deaths) by **1**.
*   **Limit:** The player is allowed a maximum of **3 total deaths** per game session.
*   **Game Over:** Upon the **3rd death**:
    1.  The `loseAnimation` (red flicker) plays.
    2.  The `score` is reset to **0**.
    3.  The `failCount` is reset to **0**.
    4.  A new goal is generated.

## Serial Commands
The game can be controlled or tested via the Serial Monitor (9600 baud):
*   `score\n`: Triggers a successful catch (increments score).
*   `fail\n`: Triggers a failure (increments death count).
*   **Acks:** The Arduino responds with `ACK: score` or `ACK: fail` upon receipt.
