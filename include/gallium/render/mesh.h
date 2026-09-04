#ifndef GALLIUM__RENDER__MESH_H
#define GALLIUM__RENDER__MESH_H
#pragma once

#include <gallium/gpu/buffer.h>
#include <gallium/gpu/commandbuffer.h>

#include <gallium/render/materiallibrary.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ga::platform
{
	class Vfs;
}

namespace ga::gpu
{
	class  Device;
	struct ASGeometry;
    class  Buffer;
	class  ComputePipeline;
    class  RenderEncoder;
	class  TransferEncoder;
	class  ShaderStructInstance;
}

namespace ga::render
{
	class Mesh;

	enum class EVertexStream
	{
		Position = 0,
		Normal,
		UV,
		Tangent,
		Joints,
		Weights,

		_Count
	};

	template<EVertexStream E>
	constexpr size_t StreamIndex = static_cast<size_t>(E);

	template<EVertexStream E> struct VertexStream {};
	template<> struct VertexStream<EVertexStream::Position> { using Type = glm::vec3;  static constexpr size_t Stride = sizeof(Type); };
	template<> struct VertexStream<EVertexStream::Normal>   { using Type = glm::vec3;  static constexpr size_t Stride = sizeof(Type); };
	template<> struct VertexStream<EVertexStream::UV>       { using Type = glm::vec2;  static constexpr size_t Stride = sizeof(Type); };
	template<> struct VertexStream<EVertexStream::Tangent>  { using Type = glm::vec3;  static constexpr size_t Stride = sizeof(Type); };
	template<> struct VertexStream<EVertexStream::Joints>   { using Type = glm::uvec4; static constexpr size_t Stride = sizeof(Type); };
	template<> struct VertexStream<EVertexStream::Weights>  { using Type = glm::vec4;  static constexpr size_t Stride = sizeof(Type); };

	struct Joint
    {
        std::string  name;
        int          parentIndex;       // -1 for root
        glm::mat4    inverseBindMatrix;

        glm::vec3    localTranslation = glm::vec3(0.f);
        glm::quat    localRotation    = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3    localScale       = glm::vec3(1.f);

        glm::mat4    globalTransform  = glm::mat4(1.f);

        glm::mat4 LocalMatrix() const
        {
            return glm::translate(glm::mat4(1.f), localTranslation)
                 * glm::mat4_cast(localRotation)
                 * glm::scale(glm::mat4(1.f), localScale);
        }
    };

	using Skeleton = std::vector<Joint>;

    struct AnimationSampler
    {
        enum class Interpolation { Step, Linear, CubicSpline };

        std::vector<float>     times;
        std::vector<glm::vec4> values;  // vec3 for T/S (w unused), vec4 quat for R
                                        // CubicSpline: [inTangent, value, outTangent] per keyframe
        Interpolation          interpolation = Interpolation::Linear;
    };

    struct AnimationChannel
    {
        enum class Path { Translation, Rotation, Scale };

        uint32_t         jointIndex;
        Path             path;
        AnimationSampler sampler;
    };

    struct AnimationClip
    {
        std::string                   name;
        float                         duration = 0.f;
        std::vector<AnimationChannel> channels;
    };

	struct Skin
	{
		Skeleton                     skeleton;
		std::vector<AnimationClip>   clips;
		std::unique_ptr<gpu::Buffer> jointMatrixBuffer;

		uint32_t FindClip(const std::string& name);
		void     Update(const ga::gpu::TransferEncoder& encoder, float time, uint32_t clipIndex);
		void     Apply(ga::gpu::Device& gpu, const ga::gpu::CommandEncoder& encoder, ga::render::Mesh& mesh);

		static void CreatePipeline(ga::platform::Vfs& vfs, ga::gpu::Device& gpu);
		static void DestroyPipeline();

	private:
		static std::unique_ptr<ga::gpu::ComputePipeline>      ms_skinPipeline;
		static std::unique_ptr<ga::gpu::ShaderStructInstance> ms_skinPC;
	};

    class Mesh
	{
	public:
		struct Primitive
		{
			std::string                      name;
			uint32_t                         materialIndex;
			uint32_t                         startIndex;
			uint32_t                         indexCount;
			std::unique_ptr<ga::gpu::Buffer> localTransformBuffer;
			glm::mat4                        localTransform;
			ga::gpu::ASGeometry*             asGeometry = nullptr;
			bool                             enabled    = true;
		};

		static constexpr size_t kMAX_BUFFERS = static_cast<size_t>(EVertexStream::_Count);

		std::array<std::unique_ptr<gpu::Buffer>, kMAX_BUFFERS> vertexBuffers          = { nullptr };
		std::array<std::unique_ptr<gpu::Buffer>, kMAX_BUFFERS> processedVertexBuffers = { nullptr };
		std::unique_ptr<gpu::Buffer>                           previousFramePositions = nullptr;
		std::unique_ptr<gpu::Buffer>                           indexBuffer   = nullptr;
		std::vector<Primitive>                                 primitives    = {};

		uint32_t indexCount  = 0;
		uint32_t vertexCount = 0;

		std::optional<Skin> skin; // present iff this mesh is skinned

		Mesh()            = default;
		Mesh(const Mesh&) = delete;
		Mesh(Mesh&&)      = default;
		~Mesh();

		bool IsSkinned() const { return skin.has_value(); }

		void UploadIndices(gpu::Device& gpu, std::span<uint32_t> data);

		template<EVertexStream E>
		void UploadStream(gpu::Device& gpu, std::span<const typename VertexStream<E>::Type> data)
		{
			vertexBuffers[StreamIndex<E>] = std::make_unique<gpu::Buffer>(gpu, gpu::BufferInfo{
				.usage       = gpu::EBufferUsage::VertexBuffer | (gpu.SupportsRaytracing() ? gpu::EBufferUsage::AccelerationStructureBuildReadonly : gpu::EBufferUsage::None),
				.size        = data.size_bytes(),
				.initialData = data.data()
			});
		}

		void UpdateAccelerationStructures(gpu::Device& gpu, const gpu::CommandEncoder* encoder = nullptr);

		void Draw(gpu::RenderEncoder& encoder, uint32_t numInstances = 1) const;
	};

	struct MeshInstance
	{
		Mesh*             mesh;
		std::vector<bool> primitiveEnablement;
		uint32_t          overrideMaterial = uint32_t(-1);

		MeshInstance(Mesh* mesh);

		struct {
			std::string name;
			float       t;
		} activeSkin = { "", 0.0f };
	};
}

#endif /* GALLIUM__RENDER__MESH_H */
