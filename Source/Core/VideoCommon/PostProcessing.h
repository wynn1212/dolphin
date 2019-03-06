// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoCommon.h"

class AbstractTexture;
class AbstractPipeline;
class AbstractShader;

namespace VideoCommon
{
class PostProcessingConfiguration
{
public:
  struct ConfigurationOption
  {
    enum OptionType
    {
      OPTION_BOOL = 0,
      OPTION_FLOAT,
      OPTION_INTEGER,
    };

    bool m_bool_value;

    std::vector<float> m_float_values;
    std::vector<s32> m_integer_values;

    std::vector<float> m_float_min_values;
    std::vector<s32> m_integer_min_values;

    std::vector<float> m_float_max_values;
    std::vector<s32> m_integer_max_values;

    std::vector<float> m_float_step_values;
    std::vector<s32> m_integer_step_values;

    OptionType m_type;

    std::string m_gui_name;
    std::string m_option_name;
    std::string m_dependent_option;
    bool m_dirty;
  };

  using ConfigMap = std::map<std::string, ConfigurationOption>;

  PostProcessingConfiguration();
  virtual ~PostProcessingConfiguration();

  // Loads the configuration with a shader
  // If the argument is "" the class will load the shader from the g_activeConfig option.
  // Returns the loaded shader source from file
  void LoadShader(const std::string& shader);
  void LoadDefaultShader();
  void SaveOptionsConfiguration();
  const std::string& GetShader() const { return m_current_shader; }
  const std::string& GetShaderCode() const { return m_current_shader_code; }
  bool IsDirty() const { return m_any_options_dirty; }
  void SetDirty(bool dirty) { m_any_options_dirty = dirty; }
  bool HasOptions() const { return m_options.size() > 0; }
  const ConfigMap& GetOptions() const { return m_options; }
  ConfigMap& GetOptions() { return m_options; }
  const ConfigurationOption& GetOption(const std::string& option) { return m_options[option]; }
  // For updating option's values
  void SetOptionf(const std::string& option, int index, float value);
  void SetOptioni(const std::string& option, int index, s32 value);
  void SetOptionb(const std::string& option, bool value);

private:
  bool m_any_options_dirty = false;
  std::string m_current_shader;
  std::string m_current_shader_code;
  ConfigMap m_options;

  void LoadOptions(const std::string& code);
  void LoadOptionsConfiguration();
};

class PostProcessing
{
public:
  PostProcessing();
  virtual ~PostProcessing();

  static std::vector<std::string> GetShaderList();
  static std::vector<std::string> GetAnaglyphShaderList();

  PostProcessingConfiguration* GetConfig() { return &m_config; }

  bool Initialize(AbstractTextureFormat format);

  void RecompileShader();
  void RecompilePipeline();

  void BlitFromTexture(const MathUtil::Rectangle<int>& dst, const MathUtil::Rectangle<int>& src,
                       const AbstractTexture* src_tex, int src_layer);

protected:
  std::string GetUniformBufferHeader() const;
  std::string GetHeader() const;
  std::string GetFooter() const;

  bool CompileVertexShader();
  bool CompilePixelShader();
  bool CompilePipeline();

  size_t CalculateUniformsSize() const;
  void FillUniformBuffer(const MathUtil::Rectangle<int>& src, const AbstractTexture* src_tex,
                         int src_layer);

  // Timer for determining our time value
  Common::Timer m_timer;
  PostProcessingConfiguration m_config;

  std::unique_ptr<AbstractShader> m_vertex_shader;
  std::unique_ptr<AbstractShader> m_pixel_shader;
  std::unique_ptr<AbstractPipeline> m_pipeline;
  AbstractTextureFormat m_framebuffer_format = AbstractTextureFormat::Undefined;
  std::vector<u8> m_uniform_staging_buffer;
};
}  // namespace VideoCommon
