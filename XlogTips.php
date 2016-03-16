<?php
class XLog
{
    const ALL 		= 0;
	const DEBUG 	= 1;
	const INFO  	= 2;	
    const NOTICE 	= 3;
    const WARNING 	= 4;
    const ERROR 	= 5;
    const CRITICAL 	= 6;
    const ALERT 	= 7;
    const EMERGENCY = 8;
	
	/**
     * 设置basePath
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
     * 获取basePath
     *
     * @return string
     */
     public static function getBasePath()
    {
        return 'the base_path';
    }
	
	/**
     * 设置当前应用名称
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
     * 获取当前应用名称
     *
     * @return string
     */
     public static function getApplication()
    {
        return 'the application';
    }

    /**
     * 设置模块目录
     * @param string $module
     *
     * @return bool
     */
    public static function setLogger($module)
    {
        return true;
    }

    /**
     * 获取最后一次设置的模块目录
     * @return string
     */
    public static function getLastLogger()
    {
        return 'the lastLogger';
    }	

    /**
     * 获得当前日志buffer中的内容
     *
     * @return array
     */
    public static function getBuffer()
    {
        return array();
    }

    /**
     * 将buffer中的日志立刻刷到硬盘或redis
     *
     * @return bool
     */
    public static function flush()
    {
        return true;
    }

    /**
     * 记录debug日志
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
     * 记录info日志
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
     * 记录notice日志
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
     * 记录warning日志
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
     * 记录error日志
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
     * 记录critical日志
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
     * 记录alert日志
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
     * 记录emergency日志
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
     * 通用日志方法
     * @param int   $level
     * @param string $message
     * @param array  $context
     * @param string $module
     */
    public static function log($level, $message, array $context = array(), $module = '')
    {

    }

    /**
     * 获取当前redis和mail网络状态
     * @return array
     */
    public static function status()
    {
        return array();
    }

    /**
     * 重置redis和mail状态
     * @return bool
     */
    public static function reset()
    {
        return true;
    }
}
