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
		TYR_COMPONENT_FIELD(&ComponentTransform::world, "World", true, true, true);
		TYR_COMPONENT_FIELD(&ComponentTransform::parentEntity, "Parent Entity", true, true, true);
	TYR_COMPONENT_END();

	TYR_COMPONENT_START(MeshComponent, 0);
		TYR_COMPONENT_FIELD(&MeshComponent::mesh, "Mesh", true, true, true);
		TYR_COMPONENT_FIELD(&MeshComponent::materialOverrides, "Material Overrides", true, true, true);
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
}
