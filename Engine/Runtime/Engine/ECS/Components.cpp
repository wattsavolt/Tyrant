#include "Components.h"
#include "ComponentReflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(Transform, 0);
		TYR_REFL_FIELD(&Transform::scale, "Scale", true, true, true);
		TYR_REFL_FIELD(&Transform::rotation, "Rotation", true, true, true);
		TYR_REFL_FIELD(&Transform::position, "Position", true, true, true);
	TYR_REFL_CLASS_END();

	TYR_COMPONENT_START(TransformComponent, 0);
		TYR_COMPONENT_FIELD(&TransformComponent::local, "Local", true, true, true);
		// Worked out from the local transform and parent, so not edited directly.
		TYR_COMPONENT_FIELD(&TransformComponent::world, "World", false, false, true);
		TYR_COMPONENT_FIELD(&TransformComponent::parentEntity, "Parent Entity", false, false, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(MeshComponent, 0);
		TYR_COMPONENT_FIELD(&MeshComponent::mesh, "Mesh", true, true, true);
		TYR_COMPONENT_FIELD(&MeshComponent::materials, "Materials", true, true, true);
		TYR_COMPONENT_FIELD(&MeshComponent::emissiveScale, "Emissive Scale", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(SkeletalMeshComponent, 0);
		TYR_COMPONENT_FIELD(&SkeletalMeshComponent::mesh, "Mesh", true, true, true);
		TYR_COMPONENT_FIELD(&SkeletalMeshComponent::materials, "Materials", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(AnimatorComponent, 0);
		TYR_COMPONENT_FIELD(&AnimatorComponent::clip, "Clip", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::fadeTime, "Fade Time", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::playRate, "Play Rate", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::loop, "Loop", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::layerClip, "Layer Clip", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::layerMaskBone, "Layer Mask Bone", true, true, true);
		TYR_COMPONENT_FIELD(&AnimatorComponent::layerWeight, "Layer Weight", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(ColliderComponent, 0);
		TYR_COMPONENT_FIELD(&ColliderComponent::shape, "Shape", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::center, "Center", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::halfExtents, "Half Extents", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::radius, "Radius", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::halfHeight, "Half Height", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::friction, "Friction", true, true, true);
		TYR_COMPONENT_FIELD(&ColliderComponent::restitution, "Restitution", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(RigidBodyComponent, 0);
		TYR_COMPONENT_FIELD(&RigidBodyComponent::motionType, "Motion Type", true, true, true);
		TYR_COMPONENT_FIELD(&RigidBodyComponent::mass, "Mass", true, true, true);
		TYR_COMPONENT_FIELD(&RigidBodyComponent::linearDamping, "Linear Damping", true, true, true);
		TYR_COMPONENT_FIELD(&RigidBodyComponent::angularDamping, "Angular Damping", true, true, true);
		TYR_COMPONENT_FIELD(&RigidBodyComponent::gravityFactor, "Gravity Factor", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(CharacterComponent, 0);
		TYR_COMPONENT_FIELD(&CharacterComponent::radius, "Radius", true, true, true);
		TYR_COMPONENT_FIELD(&CharacterComponent::height, "Height", true, true, true);
		TYR_COMPONENT_FIELD(&CharacterComponent::crouchHeight, "Crouch Height", true, true, true);
		TYR_COMPONENT_FIELD(&CharacterComponent::maxSlope, "Max Slope", true, true, true);
		TYR_COMPONENT_FIELD(&CharacterComponent::stepHeight, "Step Height", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(NavAgentComponent, 0);
		TYR_COMPONENT_FIELD(&NavAgentComponent::speed, "Speed", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::acceleration, "Acceleration", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::turnSpeed, "Turn Speed", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::arriveDistance, "Arrive Distance", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::doorBashes, "Door Bashes", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::doorBashInterval, "Door Bash Interval", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::doorBashHitDelay, "Door Bash Hit Delay", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::doorBashReach, "Door Bash Reach", true, true, true);
		TYR_COMPONENT_FIELD(&NavAgentComponent::usesStairs, "Uses Stairs", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(BoxComponent, 0);
		TYR_COMPONENT_FIELD(&BoxComponent::center, "Center", true, true, true);
		TYR_COMPONENT_FIELD(&BoxComponent::halfExtents, "Half Extents", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(DirLightComponent, 0);
		TYR_COMPONENT_FIELD(&DirLightComponent::direction, "Direction", true, true, true);
		TYR_COMPONENT_FIELD(&DirLightComponent::colour, "Colour", true, true, true);
		TYR_COMPONENT_FIELD(&DirLightComponent::intensity, "Intensity", true, true, true);
		TYR_COMPONENT_FIELD(&DirLightComponent::castsShadow, "Casts Shadow", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(PointLightComponent, 0);
		TYR_COMPONENT_FIELD(&PointLightComponent::colour, "Colour", true, true, true);
		TYR_COMPONENT_FIELD(&PointLightComponent::intensity, "Intensity", true, true, true);
		TYR_COMPONENT_FIELD(&PointLightComponent::range, "Range", true, true, true);
		TYR_COMPONENT_FIELD(&PointLightComponent::attenuation, "Attenuation", true, true, true);
		TYR_COMPONENT_FIELD(&PointLightComponent::castsShadow, "Casts Shadow", true, true, true);
		TYR_COMPONENT_FIELD(&PointLightComponent::outdoor, "Outdoor", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(DecalComponent, 0);
		TYR_COMPONENT_FIELD(&DecalComponent::material, "Material", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::opacity, "Opacity", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::fadeDepth, "Fade Depth", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::affectsColour, "Affects Colour", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::affectsNormal, "Affects Normal", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::affectsRoughness, "Affects Roughness", true, true, true);
		TYR_COMPONENT_FIELD(&DecalComponent::sortOrder, "Sort Order", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(SpotLightComponent, 0);
		TYR_COMPONENT_FIELD(&SpotLightComponent::colour, "Colour", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::intensity, "Intensity", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::range, "Range", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::coneFalloff, "Cone Falloff", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::attenuation, "Attenuation", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::castsShadow, "Casts Shadow", true, true, true);
		TYR_COMPONENT_FIELD(&SpotLightComponent::outdoor, "Outdoor", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(DoorComponent, 0);
		TYR_COMPONENT_FIELD(&DoorComponent::motion, "Motion", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::openAngle, "Open Angle", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::opensAway, "Opens Away", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::slideOffset, "Slide Offset", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::openTime, "Open Time", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::burstOpenTime, "Burst Open Time", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::lock, "Lock", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::key, "Key", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::linkedDoor, "Linked Door", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::openSound, "Open Sound", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::closeSound, "Close Sound", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::lockedSound, "Locked Sound", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::bashSound, "Bash Sound", true, true, true);
		TYR_COMPONENT_FIELD(&DoorComponent::noiseRadius, "Noise Radius", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(DoorPanelComponent, 0);
		TYR_COMPONENT_FIELD(&DoorPanelComponent::door, "Door", true, true, true);
		TYR_COMPONENT_FIELD(&DoorPanelComponent::key, "Key", true, true, true);
		TYR_COMPONENT_FIELD(&DoorPanelComponent::powered, "Powered", true, true, true);
		TYR_COMPONENT_FIELD(&DoorPanelComponent::useSound, "Use Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(LightFixtureComponent, 0);
		TYR_COMPONENT_FIELD(&LightFixtureComponent::circuit, "Circuit", true, true, true);
		TYR_COMPONENT_FIELD(&LightFixtureComponent::state, "State", true, true, true);
		TYR_COMPONENT_FIELD(&LightFixtureComponent::switchedOn, "Switched On", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(LightSwitchComponent, 0);
		TYR_COMPONENT_FIELD(&LightSwitchComponent::circuit, "Circuit", true, true, true);
		TYR_COMPONENT_FIELD(&LightSwitchComponent::switchedOn, "Switched On", true, true, true);
		TYR_COMPONENT_FIELD(&LightSwitchComponent::useSound, "Use Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(FlashlightComponent, 0);
		TYR_COMPONENT_FIELD(&FlashlightComponent::on, "On", true, true, true);
		TYR_COMPONENT_FIELD(&FlashlightComponent::offset, "Offset", true, true, true);
		TYR_COMPONENT_FIELD(&FlashlightComponent::followSpeed, "Follow Speed", true, true, true);
		TYR_COMPONENT_FIELD(&FlashlightComponent::beamRange, "Beam Range", true, true, true);
		TYR_COMPONENT_FIELD(&FlashlightComponent::beamAngle, "Beam Angle", true, true, true);
		TYR_COMPONENT_FIELD(&FlashlightComponent::toggleSound, "Toggle Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(ScentComponent, 0);
		TYR_COMPONENT_FIELD(&ScentComponent::spacing, "Spacing", true, true, true);
		TYR_COMPONENT_FIELD(&ScentComponent::lifetime, "Lifetime", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(PerceivableComponent, 0);
		TYR_COMPONENT_FIELD(&PerceivableComponent::visibility, "Visibility", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(PerceptionComponent, 0);
		TYR_COMPONENT_FIELD(&PerceptionComponent::touchRange, "Touch Range", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::nearby, "Nearby", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::nearbyRange, "Nearby Range", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::nearbyAngle, "Nearby Angle", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::trackRange, "Track Range", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::hearing, "Hearing", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::hearingScale, "Hearing Scale", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::smell, "Smell", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::smellRange, "Smell Range", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::smellMaxAge, "Smell Max Age", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::sight, "Sight", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::sightRange, "Sight Range", true, true, true);
		TYR_COMPONENT_FIELD(&PerceptionComponent::sightAngle, "Sight Angle", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(LimbCollisionComponent, 0);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::limbRadius, "Limb Radius", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::minBoneLength, "Min Bone Length", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::maxPushSpeed, "Max Push Speed", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::leftArm, "Left Arm", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::rightArm, "Right Arm", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::armTurnSpeed, "Arm Turn Speed", true, true, true);
		TYR_COMPONENT_FIELD(&LimbCollisionComponent::maxArmTurn, "Max Arm Turn", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(HealthComponent, 0);
		TYR_COMPONENT_FIELD(&HealthComponent::maxHealth, "Max Health", true, true, true);
		TYR_COMPONENT_FIELD(&HealthComponent::hurtSound, "Hurt Sound", true, true, true);
		TYR_COMPONENT_FIELD(&HealthComponent::deathSound, "Death Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(IndoorVolumeComponent, 0);
		TYR_COMPONENT_FIELD(&IndoorVolumeComponent::halfExtents, "Half Extents", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(TriggerVolumeComponent, 0);
		TYR_COMPONENT_FIELD(&TriggerVolumeComponent::halfExtents, "Half Extents", true, true, true);
		TYR_COMPONENT_FIELD(&TriggerVolumeComponent::once, "Once", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(NavmeshBoundsComponent, 0);
		TYR_COMPONENT_FIELD(&NavmeshBoundsComponent::halfExtents, "Half Extents", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(NavmeshAreaComponent, 0);
		TYR_COMPONENT_FIELD(&NavmeshAreaComponent::halfExtents, "Half Extents", true, true, true);
		TYR_COMPONENT_FIELD(&NavmeshAreaComponent::area, "Area", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(PickupComponent, 0);
		TYR_COMPONENT_FIELD(&PickupComponent::item, "Item", true, true, true);
		TYR_COMPONENT_FIELD(&PickupComponent::count, "Count", true, true, true);
		TYR_COMPONENT_FIELD(&PickupComponent::stackLimit, "Stack Limit", true, true, true);
		TYR_COMPONENT_FIELD(&PickupComponent::document, "Document", true, true, true);
		TYR_COMPONENT_FIELD(&PickupComponent::pickupSound, "Pickup Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(AmbientSoundComponent, 0);
		TYR_COMPONENT_FIELD(&AmbientSoundComponent::sound, "Sound", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(InventoryComponent, 0);
		TYR_COMPONENT_FIELD(&InventoryComponent::items, "Items", true, true, true);
		TYR_COMPONENT_FIELD(&InventoryComponent::slots, "Slots", true, true, true);
		TYR_COMPONENT_FIELD(&InventoryComponent::discardSpentKeys, "Discard Spent Keys", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(DocumentsComponent, 0);
		TYR_COMPONENT_FIELD(&DocumentsComponent::documents, "Documents", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(ActivatorComponent, 0);
		TYR_COMPONENT_FIELD(&ActivatorComponent::kind, "Kind", true, true, true);
		TYR_COMPONENT_FIELD(&ActivatorComponent::action, "Action", true, true, true);
	TYR_COMPONENT_END();
}
