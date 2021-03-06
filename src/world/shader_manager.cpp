/*
 * Copyright (c) 2012-2017 Daniele Bartolini and individual contributors.
 * License: https://github.com/dbartolini/crown/blob/master/LICENSE
 */

#include "core/containers/array.h"
#include "core/containers/hash_map.h"
#include "core/filesystem/reader_writer.h"
#include "core/strings/string.h"
#include "resource/resource_manager.h"
#include "resource/shader_resource.h"
#include "world/shader_manager.h"

namespace crown
{
ShaderManager::ShaderManager(Allocator& a)
	: _shader_map(a)
{
}

void* ShaderManager::load(File& file, Allocator& a)
{
	BinaryReader br(file);
	u32 version;
	br.read(version);
	CE_ASSERT(version == RESOURCE_VERSION_SHADER, "Wrong version");

	u32 num;
	br.read(num);

	ShaderResource* sr = CE_NEW(a, ShaderResource)(a);
	array::resize(sr->_data, num);

	for (u32 i = 0; i < num; ++i)
	{
		u32 shader_name;
		br.read(shader_name);

		u64 render_state;
		br.read(render_state);

		u32 vs_code_size;
		br.read(vs_code_size);
		const bgfx::Memory* vsmem = bgfx::alloc(vs_code_size);
		br.read(vsmem->data, vs_code_size);

		u32 fs_code_size;
		br.read(fs_code_size);
		const bgfx::Memory* fsmem = bgfx::alloc(fs_code_size);
		br.read(fsmem->data, fs_code_size);

		sr->_data[i].name._id = shader_name;
		sr->_data[i].state = render_state;
		sr->_data[i].vsmem = vsmem;
		sr->_data[i].fsmem = fsmem;
	}

	return sr;
}

void ShaderManager::online(StringId64 id, ResourceManager& rm)
{
	const ShaderResource* shader = (ShaderResource*)rm.get(RESOURCE_TYPE_SHADER, id);

	for (u32 i = 0; i < array::size(shader->_data); ++i)
	{
		const ShaderResource::Data& data = shader->_data[i];

		bgfx::ShaderHandle vs = bgfx::createShader(data.vsmem);
		CE_ASSERT(bgfx::isValid(vs), "Failed to create vertex shader");
		bgfx::ShaderHandle fs = bgfx::createShader(data.fsmem);
		CE_ASSERT(bgfx::isValid(fs), "Failed to create fragment shader");
		bgfx::ProgramHandle program = bgfx::createProgram(vs, fs, true);
		CE_ASSERT(bgfx::isValid(program), "Failed to create GPU program");

		add_shader(data.name, data.state, program);
	}
}

void ShaderManager::offline(StringId64 id, ResourceManager& rm)
{
	const ShaderResource* shader = (ShaderResource*)rm.get(RESOURCE_TYPE_SHADER, id);

	for (u32 i = 0; i < array::size(shader->_data); ++i)
	{
		const ShaderResource::Data& data = shader->_data[i];

		ShaderData sd;
		sd.state = BGFX_STATE_DEFAULT;
		sd.program = BGFX_INVALID_HANDLE;
		sd = hash_map::get(_shader_map, data.name, sd);

		bgfx::destroyProgram(sd.program);

		hash_map::remove(_shader_map, data.name);
	}
}

void ShaderManager::unload(Allocator& a, void* res)
{
	CE_DELETE(a, (ShaderResource*)res);
}

void ShaderManager::add_shader(StringId32 name, u64 state, bgfx::ProgramHandle program)
{
	ShaderData sd;
	sd.state = state;
	sd.program = program;
	hash_map::set(_shader_map, name, sd);
}

void ShaderManager::submit(StringId32 shader_id, u8 view_id)
{
	CE_ASSERT(hash_map::has(_shader_map, shader_id), "Shader not found");
	ShaderData sd;
	sd.state = BGFX_STATE_DEFAULT;
	sd.program = BGFX_INVALID_HANDLE;
	sd = hash_map::get(_shader_map, shader_id, sd);

	bgfx::setState(sd.state);
	bgfx::submit(view_id, sd.program);
}

} // namespace crown
