<?php
class XLog
{
    const ALL 	= 0;
	const DEBUG = 1;
	const INFO  = 2;	
    const NOTICE =3
    const WARNING = 4
    const ERROR = 5;
    const CRITICAL = 6;
    const ALERT = 7;
    const EMERGENCY =8;
	
	/**
     * ����basePath
     *
     * @param $basePath
     *
     * @return bool
     */
	public static function setBasePath($basePath)
    {
        return true;
    }

    /**
     * ��ȡbasePath
     *
     * @return string
     */
     public static function getBasePath()
    {
        return 'the base_path';
    }
	
	/**
     * ���õ�ǰӦ������
     *
     * @param $application
     *
     * @return bool
     */
	public static function setApplication($application)
    {
        return TRUE;
    }

    /**
     * ��ȡ��ǰӦ������
     *
     * @return string
     */
     public static function getApplication()
    {
        return 'the application';
    }

    /**
     * ����ģ��Ŀ¼
     * @param string $module
     *
     * @return bool
     */
    public static function setLogger($module)
    {
        return true;
    }

    /**
     * ��ȡ���һ�����õ�ģ��Ŀ¼
     * @return string
     */
    public static function getLastLogger()
    {
        return 'the lastLogger';
    }	

    /**
     * ��õ�ǰ��־buffer�е�����
     *
     * @return array
     */
    public static function getBuffer()
    {
        return array();
    }

    /**
     * ��buffer�е���־����ˢ��Ӳ�̻�redis
     *
     * @return bool
     */
    public static function flush()
    {
        return true;
    }

    /**
     * ��¼debug��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function debug($message, array $context = array(), $module = '')
    {
        #$level = self::DEBUG
    }

    /**
     * ��¼info��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function info($message, array $context = array(), $module = '')
    {
        #$level = self::INFO
    }

    /**
     * ��¼notice��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function notice($message, array $context = array(), $module = '')
    {
        #$level = self::NOTICE
    }

    /**
     * ��¼warning��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function warning($message, array $context = array(), $module = '')
    {
        #$level = self::WARNING
    }

    /**
     * ��¼error��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function error($message, array $context = array(), $module = '')
    {
        #$level = self::ERROR
    }

    /**
     * ��¼critical��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function critical($message, array $context = array(), $module = '')
    {
        #$level = self::CRITICAL
    }

    /**
     * ��¼alert��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function alert($message, array $context = array(), $module = '')
    {
        #$level = self::ALERT
    }

    /**
     * ��¼emergency��־
     *
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function emergency($message, array $context = array(), $module = '')
    {
        #$level = self::EMERGENCY
    }

    /**
     * ͨ����־����
     * @param int   $level
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function log($level, $message, array $context = array(), $module = '')
    {

    }
}
