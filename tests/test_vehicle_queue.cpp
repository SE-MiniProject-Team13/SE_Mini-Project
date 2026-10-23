
#include "vehicle_queue.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

// Create a vehicle on the heap because VehicleQueue owns
// successfully enqueued vehicles.
Vehicle* createVehicle(bool emergency = false) {
    Vehicle* vehicle = new Vehicle{};
    vehicle->isEmergency = emergency;
    return vehicle;
}

void test1_emptyQueue() {
    VehicleQueue queue(3);

    assert(queue.isEmpty());
    assert(queue.size() == 0);
    assert(queue.front() == nullptr);
}

void test2_dequeueFromEmptyQueue() {
    VehicleQueue queue(3);

    assert(queue.dequeue() == nullptr);
    assert(queue.dequeue() == nullptr);
    assert(queue.isEmpty());
}

void test3_rejectNullVehicle() {
    VehicleQueue queue(3);

    assert(!queue.enqueue(nullptr));
    assert(queue.isEmpty());
    assert(queue.size() == 0);
}

void test4_rejectInsertionWhenFull() {
    VehicleQueue queue(1);

    assert(queue.enqueue(createVehicle()));
    assert(queue.isFull());

    Vehicle* rejected = createVehicle();
    assert(!queue.enqueue(rejected));
    delete rejected;

    assert(queue.size() == 1);
    assert(queue.getBlockedCount() == 1);
}

void test5_capacityOfOne() {
    VehicleQueue queue(1);

    assert(queue.getCapacity() == 1);
    assert(queue.enqueue(createVehicle()));

    Vehicle* removed = queue.dequeue();
    assert(removed != nullptr);
    delete removed;

    assert(queue.isEmpty());
}

void test6_rejectZeroCapacity() {
    bool rejected = false;

    try {
        VehicleQueue queue(0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
}

void test7_emergencyPriority() {
    VehicleQueue queue(3);

    Vehicle* normal = createVehicle(false);
    Vehicle* emergency = createVehicle(true);

    assert(queue.enqueue(normal));
    assert(queue.enqueue(emergency));

    Vehicle* first = queue.dequeue();
    assert(first == emergency);
    delete first;

    Vehicle* second = queue.dequeue();
    assert(second == normal);
    delete second;
}

void test8_clearQueue() {
    VehicleQueue queue(3);

    assert(queue.enqueue(createVehicle()));
    assert(queue.enqueue(createVehicle(true)));

    queue.clear();

    assert(queue.isEmpty());
    assert(queue.size() == 0);
    assert(queue.front() == nullptr);
    assert(queue.dequeue() == nullptr);
}

void test9_repeatedEnqueueDequeue() {
    VehicleQueue queue(2);

    for (int i = 0; i < 100; ++i) {
        assert(queue.enqueue(createVehicle(i % 2 == 0)));

        Vehicle* vehicle = queue.dequeue();
        assert(vehicle != nullptr);
        delete vehicle;

        assert(queue.isEmpty());
    }
}

void test10_repeatedClearAndReuse() {
    VehicleQueue queue(3);

    for (int i = 0; i < 20; ++i) {
        assert(queue.enqueue(createVehicle()));
        assert(queue.enqueue(createVehicle(true)));

        queue.clear();

        assert(queue.isEmpty());
        assert(queue.size() == 0);
    }

    // The queue should remain usable after being cleared.
    assert(queue.enqueue(createVehicle()));
    Vehicle* vehicle = queue.dequeue();
    assert(vehicle != nullptr);
    delete vehicle;
}

int main() {
    test1_emptyQueue();
    test2_dequeueFromEmptyQueue();
    test3_rejectNullVehicle();
    test4_rejectInsertionWhenFull();
    test5_capacityOfOne();
    test6_rejectZeroCapacity();
    test7_emergencyPriority();
    test8_clearQueue();
    test9_repeatedEnqueueDequeue();
    test10_repeatedClearAndReuse();

    std::cout << "All 10 vehicle queue tests passed.\n";
    return 0;
}
