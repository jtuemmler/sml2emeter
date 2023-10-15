#ifndef WEBCONFPARAMETER_H
#define WEBCONFPARAMETER_H

#include <IotWebConf.h>

/**
 * Helper class to simplify the definition of user-parameters in IotWebConf.
 */
class WebConfParameter {
public:
  /**
   * @brief Constructor
   * 
   * @param parent        Parent of this parameter. Once created, the paramter wil be automatically added to the parent.
   * @param label         Displayable label.
   * @param id            Internal id of the parameter.
   * @param length        Length of the input field.
   * @param type          Type of the input field ("text", "number")
   * @param defaultValue  Default value.
   * @param customHtml    Custom html (eg. to set boundaries).
   */
  WebConfParameter(IotWebConf &parent, const char* label, const char* id, int length,
    const char* type = "text", const char* defaultValue = NULL, const char* customHtml = NULL)
  {
    _length = length;
    _pBuffer = new char[length + 1];
    memset(_pBuffer, 0, length + 1);
    if (defaultValue != NULL)
    {
      strncpy(_pBuffer, defaultValue, length);
    }
    _pParameter = new IotWebConfParameter(label, id, _pBuffer, length, type, defaultValue, defaultValue, customHtml, true);
    parent.addParameter(_pParameter);
  }

  /**
   * @brief Overloaded constructor to create a separator.
   * 
   * @param parent        Parent of this parameter. Once created, the paramter wil be automatically added to the parent.
   */
  WebConfParameter(IotWebConf &parent, const char *label) : _length(0), _pBuffer(NULL)
  {
    _pParameter = new IotWebConfSeparator(label);
    parent.addParameter(_pParameter);
  }

  /**
   * @brief Destructor
   */
  ~WebConfParameter()
  {
    delete _pParameter;
    if (_pBuffer != NULL) {
      delete[] _pBuffer;
    }
  }

  /**
   * @brief
   */
  IotWebConfParameter *get()
  {
    return _pParameter;  
  }
  
  /**
   * @brief Checks, whether the parameter is empty.
   */
  bool isEmpty()
  {
    return _pBuffer[0] == 0;
  }

  /**
   * @brief Returns the current value of the parameter as text.
   */
  const char* getText()
  {
    return _pBuffer;
  }

  /**
   * @brief Returns the current value of the parameter as integer.
   */
  int getInt()
  {
    return isEmpty() ? 0 : atoi(_pBuffer);
  }

  /**
   * @brief Returns the current value of the parameter as float.
   */
  float getFloat()
  {
    return isEmpty() ? 0.0f : (float)atof(_pBuffer);
  }

  /**
   * @brief Sets the current value of the parameter.
   */
  void setText(const char* text)
  {
    strncpy(_pBuffer, text, _length);
  }

  /**
   * @brief Sets the current value of the parameter.
   */
  void setInt(int value)
  {
    itoa(value, _pBuffer, 10);
  }
  
private:
  int _length;
  char *_pBuffer;
  IotWebConfParameter *_pParameter;
};

#endif // WEBCONFPARAMETER_H
