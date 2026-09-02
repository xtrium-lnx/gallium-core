#include <gallium/platform/platform.h>
#include <gallium/platform/vfs.h>
#include <gallium/platform/input.h>
#include <gallium/gpu/device.h>
#include <gallium/gpu/buffer.h>
#include <gallium/gpu/commandbuffer.h>
#include <gallium/gpu/descriptorregistry.h>
#include <gallium/gpu/image.h>
#include <gallium/gpu/graphicspipeline.h>
#include <gallium/gpu/shadermodule.h>
#include <gallium/gpu/shadermodule_reflection.h>
#include <gallium/assets/assetmanager.h>

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct TriangleVertex
{
	glm::vec2 position;
	glm::vec2 uv;
};

const std::array triangleVertices = {
	TriangleVertex { {-0.5f,  0.5f }, { 0.0f, 0.0f } },
	TriangleVertex { {-0.5f, -0.5f }, { 0.0f, 1.0f } },
	TriangleVertex { { 0.5f, -0.5f }, { 1.0f, 1.0f } },
	TriangleVertex { { 0.5f,  0.5f }, { 1.0f, 0.0f } }
};

const std::array triangleIndices = { 0u, 1u, 2u, 0u, 2u, 3u };

int main(int argc, char** argv)
{
	auto platform = std::make_unique<ga::platform::Platform>(ga::platform::PlatformDesc {
		.appName              = "Test Gallium App",
		.appVersion           = { 0, 1, 0 },
		.requestedSurfaceSize = { 1600, 900 }
	});
	platform->Input().OnPressed(ga::platform::EKey::Escape, [&]() { platform->RequestExit(); });

	auto gpu    = std::make_unique<ga::gpu::Device>(*platform);
	auto assets = std::make_unique<ga::assets::AssetManager>(*platform);


	std::unique_ptr<ga::gpu::Image> image;
	{
		auto imageData = assets->Load<ga::assets::ImageData>("/assets/cage.png");
		image = std::make_unique<ga::gpu::Image>(*gpu, ga::gpu::ImageDesc {
			.type        = ga::gpu::EImageType::Image2D,
			.format      = imageData.format,
			.width       = imageData.width,
			.height      = imageData.height,
			.usage       = ga::gpu::EImageUsage::Sampled,
			.initialData = imageData.pixels.data()
		});
	}

	auto vb = std::make_unique<ga::gpu::Buffer>(*gpu, ga::gpu::EBufferUsage::VertexBuffer, triangleVertices);
	auto ib = std::make_unique<ga::gpu::Buffer>(*gpu, ga::gpu::EBufferUsage::IndexBuffer,  triangleIndices);

	ga::gpu::ShaderStructDesc trianglePC;
	ga::gpu::ShaderStructDesc triangleOD;

	std::unique_ptr<ga::gpu::GraphicsPipeline> pipeline;
	{
		auto shader = platform->Vfs().Open("/shaders/triangle.slang.spv")
			.and_then([](auto f) { return f.ReadBytes(); })
			.transform([&gpu](auto bytes) { return std::make_unique<ga::gpu::ShaderModule>(*gpu, bytes.data(), bytes.size()); })
			.value();

		trianglePC = *shader->GetStruct("PushConstants");
		triangleOD = *shader->GetStruct("ObjectData");

		pipeline = std::make_unique<ga::gpu::GraphicsPipeline>(*gpu, ga::gpu::GraphicsPipelineInfo {
			.stages = {
				{ *shader, ga::gpu::EShaderStage::Vertex,   "vs_main" },
				{ *shader, ga::gpu::EShaderStage::Fragment, "fs_main" },
			},
			.vertexInput = {
				.bindings = {
					{ 0u, sizeof(TriangleVertex), ga::gpu::EVertexBindingRate::Vertex }
				},
				.attributes = {
					{ 0, 0, ga::gpu::EFormat::R32G32_SFloat, offsetof(TriangleVertex, position) },
					{ 1, 0, ga::gpu::EFormat::R32G32_SFloat, offsetof(TriangleVertex, uv) }
				}
			},
			.attachmentFormats = {
				.colorFormats = { ga::gpu::EFormat::B8G8R8A8_SRGB }
			}
		});
	}

	auto triangleODData = triangleOD.Instantiate();
	triangleODData["viewProj"] =
		glm::perspective(glm::radians(50.0f), 16.f/9.f, 0.1f, 100.0f)
		* glm::lookAt(glm::vec3 { 1.0f, 1.0f, 3.0f }, glm::vec3{ 0.0f }, glm::vec3 { 0.0f, 1.0f, 0.0f });
	auto odBuffer = std::make_unique<ga::gpu::Buffer>(*gpu, ga::gpu::EBufferUsage::UniformBuffer, triangleODData);

	auto trianglePCData = trianglePC.Instantiate();
	trianglePCData["data"] = odBuffer->GetGpuAddress();
	trianglePCData["model"] = glm::identity<glm::mat4>();
	trianglePCData["textureId"] = image->GetDescriptorIndex().slot;

	do
	{
		auto acquiredImage = gpu->AcquireNextImage();

		auto cb = gpu->AcquireCommandBuffer();

		cb->Record([&](const ga::gpu::CommandEncoder& encoder) {
			encoder.Transition(acquiredImage.image, ga::gpu::EImageLayout::ColorAttachmentOptimal);

			auto beginRenderingInfo = ga::gpu::BeginRenderingInfo {
				.pipeline = *pipeline,
				.renderArea = {
					.offset = { 0, 0 },
					.extent = acquiredImage.image.Size()
				},
				.colorAttachments = {
					{ .image = acquiredImage.image }
				},
			};

			encoder.Render(beginRenderingInfo, [&](const ga::gpu::RenderEncoder& render) {
				render.SetViewport({ 0.0f, 0.0f }, acquiredImage.image.Size());
				render.SetScissor({ 0, 0 }, acquiredImage.image.Size());
				render.BindVertexBuffers(0, { { vb.get(), 0 } });
				render.BindIndexBuffer(ib.get());
				render.PushConstants(trianglePCData);
				render.DrawIndexed(6);
			});
			
			encoder.Transition(acquiredImage.image, ga::gpu::EImageLayout::PresentSrc);
		}, ga::gpu::ECommandBufferUsage::OneTimeSubmit);

		gpu->Submit(cb, { acquiredImage.presentComplete }, acquiredImage.renderComplete);
		gpu->ReleaseCommandBuffer(cb);

		gpu->Present({ acquiredImage.renderComplete });
		platform->PollEvents();
	} while (!platform->IsExitRequested());

	gpu->WaitIdle();
	
	odBuffer.reset();
	pipeline.reset();
	image.reset();
	ib.reset();
	vb.reset();
	gpu.reset();
	platform.reset();
	return 0;
}
