
#include "vehicle_queue.h"

#include <cassert>
#include <iostream>

void testEmptyQueue() {
    VehicleQueue queue(3);

    assert(queue.isEmpty());
    assert(queue.size() == 0);
    assert(queue.front() == nullptr);
    assert(queue.dequeue() == nullptr);
}

void testFifoOrder() {
    VehicleQueue queue(3);

    Vehicle* v1 = new Vehicle{1, Direction::North, false, 1};
    Vehicle* v2 = new Vehicle{2, Direction::North, false, 2};

    assert(queue.enqueue(v1));
    assert(queue.enqueue(v2));

    Vehicle* first = queue.dequeue();
    assert(first != nullptr && first->id == 1);
    delete first;

    Vehicle* second = queue.dequeue();
    assert(second != nullptr && second->id == 2);
    delete second;

    assert(queue.isEmpty());
}

void testEmergencyPriority() {
    VehicleQueue queue(3);

    Vehicle* normal = new Vehicle{1, Direction::East, false, 1};
    Vehicle* emergency = new Vehicle{2, Direction::East, true, 2};

    assert(queue.enqueue(normal));
    assert(queue.enqueue(emergency));

    Vehicle* first = queue.dequeue();
    assert(first != nullptr && first->id == 2);
    delete first;

    Vehicle* second = queue.dequeue();
    assert(second != nullptr && second->id == 1);
    delete second;
}

void testCapacityAndBlockedCount() {
    VehicleQueue queue(1);

    assert(queue.enqueue(
        new Vehicle{1, Direction::South, false, 0}));

    assert(queue.isFull());

    Vehicle* rejected =
        new Vehicle{2, Direction::South, false, 1};

    assert(!queue.enqueue(rejected));
    assert(queue.getBlockedCount() == 1);

    // Rejected vehicles remain owned by the caller.
    delete rejected;

    Vehicle* removed = queue.dequeue();
    delete removed;

    assert(queue.isEmpty());
}

void testWaitingTime() {
    VehicleQueue queue(2);

    Vehicle* vehicle =
        new Vehicle{1, Direction::West, false, 10};

    assert(queue.enqueue(vehicle));

    Vehicle* departed = queue.dequeue();
    assert(departed != nullptr);

    departed->departureTick = 17;
    departed->waitingTime =
        departed->departureTick - departed->arrivalTick;

    assert(departed->waitingTime == 7);
    delete departed;
}

void testClear() {
    VehicleQueue queue(3);

    assert(queue.enqueue(
        new Vehicle{1, Direction::North, false, 0}));

    assert(queue.enqueue(
        new Vehicle{2, Direction::North, true, 1}));

    queue.clear();

    assert(queue.isEmpty());
    assert(queue.size() == 0);
    assert(queue.front() == nullptr);
}

int main() {
    testEmptyQueue();
    testFifoOrder();
    testEmergencyPriority();
    testCapacityAndBlockedCount();
    testWaitingTime();
    testClear();

    std::cout << "All vehicle queue tests passed.\n";
    return 0;
}
