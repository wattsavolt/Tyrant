#include "TransferPass.h"
#include "Rendering/RenderData.h"
#include "Rendering/RenderRegistry.h"
#include "Rendering/RenderGraphBuilder.h"
#include "Rendering/RenderResources.h"
#include "Rendering/RenderFrame.h"
#include "RenderTransfer/GpuTransferUtil.h"

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
		m_Data = args.data;
		m_Registry = args.registry;
		m_Resources = args.resources;
		m_RenderFrame = args.renderFrame;
	}

	void TransferPass::Setup(RenderGraphBuilder& builder)
	{
		const RenderBufferHandle buffers[] = {
			m_Resources->meshBuffer, m_Resources->meshLODBuffer, m_Resources->meshletBuffer,
			m_Resources->vertexBuffer, m_Resources->indexBuffer, m_Resources->meshInstanceBuffer,
			m_Resources->materialBuffer, m_Resources->directionalLightBuffer, m_Resources->pointLightBuffer,
			m_Resources->spotLightBuffer, m_Resources->sceneInfoBuffer,
			m_Resources->guiVertexBuffer, m_Resources->guiIndexBuffer
		};

		for (RenderBufferHandle buffer : buffers)
		{
			builder.WriteBuffer(m_Registry->GetBuffer(buffer), PIPELINE_STAGE_TRANSFER_BIT, BARRIER_ACCESS_TRANSFER_WRITE_BIT);
		}
	}

	void TransferPass::Execute(CommandList& cmdList)
	{
		if (!m_Data->assetBufferUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToBuffers(cmdList, m_Data->assetBufferUploadRequests.Data(), m_Data->assetBufferUploadRequests.Size());
		}

		if (m_Data->activeScene)
		{
			Scene& scene = m_Data->scenes[m_Data->activeScene.h];
			if (!scene.frameUploadRequests.IsEmpty())
			{
				GpuTransferUtil::UploadToBuffers(cmdList, scene.frameUploadRequests.Data(), scene.frameUploadRequests.Size());
			}
		}

		if (!m_Data->textureUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToTextures(cmdList, m_Data->textureUploadRequests.Data(), m_Data->textureUploadRequests.Size());
		}

		// Per-buffered-frame requests (RendererAPI::SubmitGUIDrawData / AddTextureUploadRequest) -
		// separate from RenderData's asset-level lists above since these come from things
		// re-submitted every frame (GUI vertex/index data, its font atlas) rather than once per
		// asset load.
		if (!m_RenderFrame->frameBufferUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToBuffers(cmdList, m_RenderFrame->frameBufferUploadRequests.Data(), m_RenderFrame->frameBufferUploadRequests.Size());
		}

		if (!m_RenderFrame->textureUploadRequests.IsEmpty())
		{
			GpuTransferUtil::UploadToTextures(cmdList, m_RenderFrame->textureUploadRequests.Data(), m_RenderFrame->textureUploadRequests.Size());
		}
	}
}
