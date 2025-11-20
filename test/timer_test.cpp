#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <cassert>
#include <thread>
#include <chrono>

using namespace websrv;

int main() {
    Poller poller;
    
    LOG("Test 1: One-shot timer");
    auto timer1 = poller.createTimer(100, false); // 100ms, one-shot
    
    // Should not be expired immediately
    assert(!poller.isTimerExpired(timer1));
    LOG("✓ Timer not expired immediately");
    
    // Wait and poll
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    poller.poll(0); // Process timers
    
    // Should be expired now
    assert(poller.isTimerExpired(timer1));
    LOG("✓ Timer expired after delay");
    
    // Reset should clear expired flag
    poller.resetTimer(timer1);
    assert(!poller.isTimerExpired(timer1));
    LOG("✓ Reset cleared expired flag");
    
    // Destroy timer
    poller.destroyTimer(timer1);
    LOG("✓ Timer destroyed");
    
    LOG("\nTest 2: Repeating timer");
    auto timer2 = poller.createTimer(50, true); // 50ms, repeating
    
    int expire_count = 0;
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        poller.poll(0);
        
        if (poller.isTimerExpired(timer2)) {
            expire_count++;
            poller.resetTimer(timer2);
            LOG("✓ Timer expired (count: ", expire_count, ")");
        }
    }
    
    assert(expire_count >= 3); // Should expire at least 3 times
    LOG("✓ Repeating timer works (expired ", expire_count, " times)");
    
    poller.destroyTimer(timer2);
    
    LOG("\nTest 3: Multiple timers");
    auto timerA = poller.createTimer(50, false);
    auto timerB = poller.createTimer(100, false);
    auto timerC = poller.createTimer(150, false);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    poller.poll(0);
    
    assert(poller.isTimerExpired(timerA));
    assert(!poller.isTimerExpired(timerB));
    assert(!poller.isTimerExpired(timerC));
    LOG("✓ Timer A expired, B and C not expired");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    poller.poll(0);
    
    assert(poller.isTimerExpired(timerB));
    assert(!poller.isTimerExpired(timerC));
    LOG("✓ Timer B expired, C not expired");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    poller.poll(0);
    
    assert(poller.isTimerExpired(timerC));
    LOG("✓ Timer C expired");
    
    poller.destroyTimer(timerA);
    poller.destroyTimer(timerB);
    poller.destroyTimer(timerC);
    
    LOG("\n✅ All timer tests passed!");
    return 0;
}