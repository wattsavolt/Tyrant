#include "TransferPass.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "Rendering/RenderFrame.h"
#include "Rendering/RenderData.h"
#include "RenderTransfer/GpuTransferUtil.h"
#include "RenderTransfer/UploadRequest.h"
#include "RenderResource/Texture.h"

namespace tyr
{
	TransferPass::TransferPass(const TransferPassArgs& args)
	{
		Recreate(args);
	}

	TransferPass::~TransferPass()
	{

	}

	void TransferPass::Recreate(const TransferPassArgs& args)
	{
		m_Device = args.device;
		m_Registry = args.registry;
		m_Resources = args.resources;
	}

	void TransferPass::Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame)
	{
		const RenderBufferHandle buffers[] = {
			m_Resources->meshBuffer, m_Resources->meshLODBuffer, m_Resources->meshletBuffer,
			m_Resources->vertexBuffer, m_Resources->indexBuffer, m_Resources->meshInstanceBuffer,
			m_Resources->materialBuffer, m_Resources->directionalLightBuffer, m_Resources->pointLightBuffer,
			m_Resources->spotLightBuffer, m_Resources->sceneInfoBuffer,
			m_Resources->guiVertexBuffer, m_Resources->guiIndexBuffer,
			// GPU-driven instance culling - this frame's active instance list and the atomic
			// draw counter's reset-to-zero, both uploaded from RenderAsync's merge step.
			m_Resources->activeMeshInstanceIndexBuffer, m_Resources->drawCountBuffer,
			// Same merge step, for the TLAS build's instance data.
			m_Resources->tlasInstanceBuffer,
			// This tick's light-index -> shadow-slot lookup, built once ShadowRTPass's selection
			// is known.
			m_Resources->shadowLightSlotMapBuffer,
			// The active scene's lights this tick.
			m_Resources->lightIndexBuffer
		};

		for (RenderBufferHandle buffer : buffers)
		{
			builder.WriteBuffer(m_Registry->GetBuffer(buffer), PIPELINE_STAGE_TRANSFER_BIT, BARRIER_ACCESS_TRANSFER_WRITE_BIT);
		}

		for (const TextureUploadRequest& request : renderFrame.textureUploadRequests)
		{
			builder.WriteTexture(m_Registry->GetTexture(request.dstTexture), PIPELINE_STAGE_TRANSFER_BIT, BARRIER_ACCESS_TRANSFER_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
		}
	}

	void TransferPass::Execute(CommandList& cmdList, RenderFrame& renderFrame, RenderData& data)
	{
		// Read straight off RenderFrame, not a RenderData-owned copy - nothing else touches this
		// slot's RenderFrame while this runs, so a plain read is just as safe as a copy without
		// the pointless duplication.
		if (!renderFrame.assetBufferUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToBuffers(cmdList, renderFrame.assetBufferUploadRequests.Data(), renderFrame.assetBufferUploadRequests.Size());
		}

		if (!renderFrame.frameBufferUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToBuffers(cmdList, renderFrame.frameBufferUploadRequests.Data(), renderFrame.frameBufferUploadRequests.Size());
		}

		if (!renderFrame.textureUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToTextures(cmdList, renderFrame.textureUploadRequests.Data(), renderFrame.textureUploadRequests.Size());
		}

		// Worker-owned, not RenderFrame.
		if (!data.workerUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToBuffers(cmdList, data.workerUploadRequests.Data(), data.workerUploadRequests.Size());
		}
	}
}
