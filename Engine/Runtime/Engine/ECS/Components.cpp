#include "Components.h"
#include "ComponentReflection.h"

namespace tyr
{
	TYR_REFL_CLASS_START(Transform, 0);
		TYR_REFL_FIELD(&Transform::scale, "Scale", true, true, true);
		TYR_REFL_FIELD(&Transform::rotation, "Rotation", true, true, true);
		TYR_REFL_FIELD(&Transform::position, "Position", true, true, true);
	TYR_REFL_CLASS_END();

	TYR_COMPONENT_START(ComponentTransform, 0);
		TYR_COMPONENT_FIELD(&ComponentTransform::local, "Local", true, true, true);
		// Worked out from the local transform and parent, so not edited directly.
		TYR_COMPONENT_FIELD(&ComponentTransform::world, "World", false, false, true);
		TYR_COMPONENT_FIELD(&ComponentTransform::parentEntity, "Parent Entity", false, false, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(MeshComponent, 0);
		TYR_COMPONENT_FIELD(&MeshComponent::mesh, "Mesh", true, true, true);
		TYR_COMPONENT_FIELD(&MeshComponent::materials, "Materials", true, true, true);
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
	TYR_COMPONENT_END();
}
