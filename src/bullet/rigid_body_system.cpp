#include "bullet/rigid_body_system.h"

#include "engine/component_registry.h"
#include "engine/entity_filter.h"
#include "bullet/bullet_engine.h"
#include "scripting/luabind.h"
#include "common/transform.h"

#include <iostream>

#include <OgreVector3.h>
#include <OgreQuaternion.h>

using namespace thrive;

////////////////////////////////////////////////////////////////////////////////
// RigidBodyComponent
////////////////////////////////////////////////////////////////////////////////


static void
RigidBodyComponent_touch(
    RigidBodyComponent* self
) {
    return self->m_staticProperties.touch();
}

static void
RigidBodyComponent_setDynamicProperties(
    RigidBodyComponent* self,
    Ogre::Vector3 position,
    Ogre::Quaternion rotation,
    Ogre::Vector3 linearVelocity,
    Ogre::Vector3 angularVelocity
) {
    self->m_dynamicProperties.workingCopy().position = btVector3(position.x,position.y,position.z);
    self->m_dynamicProperties.workingCopy().rotation = btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);
    self->m_dynamicProperties.workingCopy().linearVelocity = btVector3(linearVelocity.x,linearVelocity.y,linearVelocity.z);
    self->m_dynamicProperties.workingCopy().angularVelocity = btVector3(angularVelocity.x,angularVelocity.y,angularVelocity.z);
    return self->m_dynamicProperties.touch();
}


static RigidBodyComponent::StaticProperties&
RigidBodyComponent_getWorkingCopy(
    RigidBodyComponent* self
) {
    return self->m_staticProperties.workingCopy();
}

static const RigidBodyComponent::StaticProperties&
RigidBodyComponent_getLatest(
    RigidBodyComponent* self
) {
    return self->m_staticProperties.latest();
}


luabind::scope
RigidBodyComponent::luaBindings() {
    using namespace luabind;
    return class_<RigidBodyComponent, Component, std::shared_ptr<Component>>("RigidBodyComponent")
        .scope [
            def("TYPE_NAME", &RigidBodyComponent::TYPE_NAME),
            def("TYPE_ID", &RigidBodyComponent::TYPE_ID),
            class_<StaticProperties>("StaticProperties")
                .def_readwrite("shape", &StaticProperties::shape)
                /*.def_readwrite("linearVelocity", &Properties::linearVelocity)
                .def_readwrite("position", &Properties::position)
                .def_readwrite("rotation", &Properties::rotation)
                .def_readwrite("angularVelocity", &Properties::angularVelocity)*/
                .def_readwrite("restitution", &StaticProperties::restitution)
                .def_readwrite("linearFactor", &StaticProperties::linearFactor)
                .def_readwrite("angularFactor", &StaticProperties::angularFactor)
                .def_readwrite("mass", &StaticProperties::mass)
                .def_readwrite("comOffset", &StaticProperties::comOffset)
                .def_readwrite("friction", &StaticProperties::friction)
                .def_readwrite("rollingFriction", &StaticProperties::rollingFriction)
        ]
        .def(constructor<>())
        .property("latest", RigidBodyComponent_getLatest)
        .property("workingCopy", RigidBodyComponent_getWorkingCopy)
        .def("touch", RigidBodyComponent_touch)
        .def("setDynamicProperties", RigidBodyComponent_setDynamicProperties)
    ;
}

REGISTER_COMPONENT(RigidBodyComponent)


////////////////////////////////////////////////////////////////////////////////
// RigidBodyInputSystem
////////////////////////////////////////////////////////////////////////////////

struct RigidBodyInputSystem::Implementation {

    EntityFilter<
        RigidBodyComponent
    > m_entities = {true};

    std::unordered_map<EntityId, btRigidBody*> m_bodies;

    btDiscreteDynamicsWorld* m_world = nullptr;

};


RigidBodyInputSystem::RigidBodyInputSystem()
  : m_impl(new Implementation())
{
}


RigidBodyInputSystem::~RigidBodyInputSystem() {}


void
RigidBodyInputSystem::init(
    Engine* engine
) {
    System::init(engine);
    assert(m_impl->m_world == nullptr && "Double init of system");
    BulletEngine* bulletEngine = dynamic_cast<BulletEngine*>(engine);
    assert(bulletEngine != nullptr && "System requires a BulletEngine");
    m_impl->m_world = bulletEngine->world();
    m_impl->m_entities.setEngine(engine);
}


void
RigidBodyInputSystem::shutdown() {
    m_impl->m_entities.setEngine(nullptr);
    m_impl->m_world = nullptr;
    System::shutdown();
}


void
RigidBodyInputSystem::update(int) {
    for (const auto& added : m_impl->m_entities.addedEntities()) {
        EntityId entityId = added.first;
        RigidBodyComponent* rigidBodyComponent = std::get<0>(added.second);
        btDefaultMotionState* motionState =
                new btDefaultMotionState(btTransform(rigidBodyComponent->m_dynamicProperties.stable().rotation,rigidBodyComponent->m_dynamicProperties.stable().position),rigidBodyComponent->m_staticProperties.stable().comOffset);
        btRigidBody::btRigidBodyConstructionInfo rigidBodyCI = btRigidBody::btRigidBodyConstructionInfo(
            rigidBodyComponent->m_staticProperties.stable().mass, motionState, rigidBodyComponent->m_staticProperties.stable().shape.get(),rigidBodyComponent->m_staticProperties.stable().inertia);
        btRigidBody* rigidBody = new btRigidBody(rigidBodyCI);
        rigidBodyComponent->m_body = rigidBody;
        m_impl->m_bodies[entityId] = rigidBody;
        m_impl->m_world->addRigidBody(rigidBody);
    }
    for (const auto& value : m_impl->m_entities) {
        RigidBodyComponent* rigidBodyComponent = std::get<0>(value.second);
        if (rigidBodyComponent->m_staticProperties.hasChanges()) {
            btRigidBody* body = rigidBodyComponent->m_body;
            const auto& properties = rigidBodyComponent->m_staticProperties.stable();
            body->setMassProps(properties.mass, properties.inertia);
            body->setLinearFactor(properties.linearFactor);
            body->setAngularFactor(properties.angularFactor);
            body->setRestitution(properties.restitution);
            body->setCollisionShape(properties.shape.get());
            body->setFriction(properties.friction);
            body->setRollingFriction(properties.friction);
            rigidBodyComponent->m_staticProperties.untouch();
        }
        if (rigidBodyComponent->m_dynamicProperties.hasChanges()) {
            btRigidBody* body = rigidBodyComponent->m_body;
            const auto& properties = rigidBodyComponent->m_dynamicProperties.stable();
            btTransform transform;
            transform.setIdentity();
            transform.setOrigin(properties.position);
            transform.setRotation(properties.rotation);
            body->setWorldTransform(transform);
            body->setLinearVelocity(properties.linearVelocity);
            body->setAngularVelocity(properties.angularVelocity);
            rigidBodyComponent->m_dynamicProperties.untouch();
        }

    }
    for (EntityId entityId : m_impl->m_entities.removedEntities()) {
        btRigidBody* body = m_impl->m_bodies[entityId];
        m_impl->m_world->removeRigidBody(body);
        m_impl->m_bodies.erase(entityId);
    }
    m_impl->m_entities.clearChanges();
}

////////////////////////////////////////////////////////////////////////////////
// RigidBodyOutputSystem
////////////////////////////////////////////////////////////////////////////////

struct RigidBodyOutputSystem::Implementation {

    EntityFilter<
        RigidBodyComponent,
        PhysicsTransformComponent
    > m_entities;
};


RigidBodyOutputSystem::RigidBodyOutputSystem()
  : m_impl(new Implementation())
{
}


RigidBodyOutputSystem::~RigidBodyOutputSystem() {}


void
RigidBodyOutputSystem::init(
    Engine* engine
) {
    System::init(engine);
    m_impl->m_entities.setEngine(engine);
}


void
RigidBodyOutputSystem::shutdown() {
    m_impl->m_entities.setEngine(nullptr);
    System::shutdown();
}


void
RigidBodyOutputSystem::update(int) {
    for (auto& value : m_impl->m_entities.entities()) {
        RigidBodyComponent* rigidBodyComponent = std::get<0>(value.second);
        PhysicsTransformComponent* transform = std::get<1>(value.second);
        btRigidBody* rigidBody = rigidBodyComponent->m_body;
        btTransform trans = rigidBody->getWorldTransform();
        btVector3 position = trans.getOrigin();
        btQuaternion rotation = trans.getRotation();
        btVector3 velocity = rigidBody->getLinearVelocity();
        transform->m_properties.workingCopy().position = Ogre::Vector3(position.x(),position.y(),position.z());
        transform->m_properties.workingCopy().rotation = Ogre::Quaternion(rotation.w(),rotation.x(),rotation.y(),rotation.z());
        transform->m_properties.workingCopy().velocity = Ogre::Vector3(velocity.x(),velocity.y(),velocity.z());
        transform->m_properties.touch();
    }
}
