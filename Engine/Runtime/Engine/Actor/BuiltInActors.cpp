#include "ActorReflection.h"
#include "ECS/Components.h"

namespace tyr
{
	namespace
	{
		ComponentTransform CreateIdentityTransform()
		{
			ComponentTransform transform;
			transform.local.position = Vector3::c_Zero;
			transform.local.rotation = Quaternion::c_Identity;
			transform.local.scale = Vector3::c_One;
			transform.world = transform.local;
			return transform;
		}
	}

	// Starts with no mesh, which is set from the editor's properties.
	TYR_ACTOR_START(StaticMeshActor)
		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(MeshComponent{});
		root.AddComponent(BoxComponent{});
	TYR_ACTOR_END()

	TYR_ACTOR_START(DirLightActor)
		DirLightComponent light{};
		light.direction = Vector3::Normalize(Vector3(-0.4f, -0.8f, 0.4f));
		light.colour = Vector3::c_One;
		light.intensity = 3.0f;
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()

	TYR_ACTOR_START(PointLightActor)
		PointLightComponent light{};
		light.colour = Vector3::c_One;
		light.intensity = 5.0f;
		light.range = 10.0f;
		light.attenuation = Vector3(1.0f, 0.5f, 0.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()

	// Shines along its Z axis, so it's turned with the rotate gizmo.
	TYR_ACTOR_START(SpotLightActor)
		SpotLightComponent light{};
		light.colour = Vector3::c_One;
		light.intensity = 5.0f;
		light.range = 15.0f;
		light.coneFalloff = 8.0f;
		light.attenuation = Vector3(1.0f, 0.5f, 0.0f);
		light.castsShadow = true;

		ActorEntityDesc& root = actorTypeDesc.AddEntity("Root");
		root.AddComponent(CreateIdentityTransform());
		root.AddComponent(light);
	TYR_ACTOR_END()
}
