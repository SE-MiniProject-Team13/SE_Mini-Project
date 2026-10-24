
#include "vehicle_queue.h"
#include <stdexcept>

VehicleQueue::VehicleQueue(std::size_t maxCapacity)
    : normalFront(nullptr),
      normalRear(nullptr),
      emergencyFront(nullptr),
      emergencyRear(nullptr),
      currentSize(0),
      capacity(maxCapacity),
      blockedCount(0),
      maxSize(0) {
    if (maxCapacity == 0) {
        throw std::invalid_argument(
            "Queue capacity must be greater than zero"
        );
    }
}

VehicleQueue::~VehicleQueue() {
    clear();
}

bool VehicleQueue::enqueue(Vehicle* vehicle) {
    if (vehicle == nullptr) {
        return false;
    }

    if (isFull()) {
        ++blockedCount;
        return false;
    }

    Node* newNode = new Node(vehicle);

    if (vehicle->isEmergency) {
        if (emergencyRear == nullptr) {
            emergencyFront = emergencyRear = newNode;
        } else {
            emergencyRear->next = newNode;
            emergencyRear = newNode;
        }
    } else {
        if (normalRear == nullptr) {
            normalFront = normalRear = newNode;
        } else {
            normalRear->next = newNode;
            normalRear = newNode;
        }
    }

    ++currentSize;

    if (currentSize > maxSize) {
        maxSize = currentSize;
    }

    return true;
}

Vehicle* VehicleQueue::dequeue() {
    Node** selectedFront;
    Node** selectedRear;

    // Emergency vehicles have priority over normal vehicles.
    if (emergencyFront != nullptr) {
        selectedFront = &emergencyFront;
        selectedRear = &emergencyRear;
    } else if (normalFront != nullptr) {
        selectedFront = &normalFront;
        selectedRear = &normalRear;
    } else {
        return nullptr;
    }

    Node* oldNode = *selectedFront;
    Vehicle* vehicle = oldNode->vehicle;

    *selectedFront = oldNode->next;

    if (*selectedFront == nullptr) {
        *selectedRear = nullptr;
    }

    delete oldNode;
    --currentSize;

    return vehicle;
}

Vehicle* VehicleQueue::front() const {
    if (emergencyFront != nullptr) {
        return emergencyFront->vehicle;
    }

    if (normalFront != nullptr) {
        return normalFront->vehicle;
    }

    return nullptr;
}

bool VehicleQueue::isEmpty() const {
    return currentSize == 0;
}

bool VehicleQueue::isFull() const {
    return currentSize >= capacity;
}

std::size_t VehicleQueue::size() const {
    return currentSize;
}

std::size_t VehicleQueue::getCapacity() const {
    return capacity;
}

std::size_t VehicleQueue::getBlockedCount() const {
    return blockedCount;
}

std::size_t VehicleQueue::getMaxSize() const {
    return maxSize;
}

void VehicleQueue::clearNodes(Node*& front, Node*& rear) {
    while (front != nullptr) {
        Node* oldNode = front;
        front = front->next;
        delete oldNode->vehicle;
        delete oldNode;
    }

    rear = nullptr;
}

void VehicleQueue::clear() {
    clearNodes(normalFront, normalRear);
    clearNodes(emergencyFront, emergencyRear);
    currentSize = 0;
}
