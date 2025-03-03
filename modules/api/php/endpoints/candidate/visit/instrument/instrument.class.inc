<?php declare(strict_types=1);
/**
 * This implements the Visit specific instrument class
 *
 * PHP Version 7
 *
 * @category API
 * @package  Loris
 * @author   Xavier Lecours Boucher <xavier.lecours@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/aces/Loris
 */
namespace LORIS\api\Endpoints\Candidate\Visit\Instrument;

use \Psr\Http\Message\ServerRequestInterface;
use \Psr\Http\Message\ResponseInterface;
use \LORIS\api\Endpoint;

/**
 * A class for handling the request for specific instrument data.
 *
 * @category API
 * @package  Loris
 * @author   Xavier Lecours Boucher <xavier.lecours@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/aces/Loris
 */
class Instrument extends Endpoint implements \LORIS\Middleware\ETagCalculator
{
    /**
     * The requested Visit
     *
     * @var \Timepoint
     */
    private $_visit;

    /**
     * The requested Visit
     *
     * @var \NDB_BVL_instrument
     */
    private $_instrument;

    /**
     * Contructor
     *
     * @param \Timepoint          $visit      The requested visit; null for PUT
     * @param \NDB_BVL_instrument $instrument The requested instrument
     */
    public function __construct(\Timepoint $visit, \NDB_BVL_instrument $instrument)
    {
        $this->_visit      = $visit;
        $this->_instrument = $instrument;
    }

    /**
     * Return which methods are supported by this endpoint.
     *
     * @return array supported HTTP methods
     */
    protected function allowedMethods() : array
    {
        return [
            'GET',
            'PUT',
            'PATCH',
        ];
    }

    /**
     * Versions of the LORIS API which are supported by this
     * endpoint.
     *
     * @return array a list of supported API versions.
     */
    protected function supportedVersions() : array
    {
        return [
            'v0.0.3',
            'v0.0.4-dev',
        ];
    }

    /**
     * Handles a request that starts with /candidates/$candid
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    public function handle(ServerRequestInterface $request) : ResponseInterface
    {
        $pathparts = $request->getAttribute('pathparts');
        $flags     = array_search('flags', $pathparts) !== false;
        $user      = $request->getAttribute('user');

        if (!$this->_instrument->_hasAccess($user)) {
            return new \LORIS\Http\Response\JSON\Forbidden(
                'This user can not update this instrument'
            );
        }

        if ($flags) {
            // Delegate to sub-endpoints
            $handler = new Flags($this->_visit, $this->_instrument);

            return $handler->process(
                $request,
                $handler
            );
        }

        switch ($request->getMethod()) {
        case 'GET':
            return $this->_handleGET($request);

        case 'PUT':
            return $this->_handlePUT($request);

        case 'PATCH':
            return $this->_handlePATCH($request);

        case 'OPTIONS':
            return (new \LORIS\Http\Response())
                ->withHeader('Allow', $this->allowedMethods());

        default:
            return new \LORIS\Http\Response\JSON\MethodNotAllowed(
                $this->allowedMethods()
            );
        }
    }

    /**
     * Handles a GET request
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handleGET(ServerRequestInterface $request) : ResponseInterface
    {

        $version = $request->getAttribute('LORIS-API-Version');
        switch ($version) {
        case 'v0.0.3':
            $body = (new \LORIS\api\Views\Visit\Instrument(
                $this->_visit,
                $this->_instrument
            ))->toArray();
            break;
        default:
            $body = (new \LORIS\api\Views\Visit\Instrument_0_0_4_Dev(
                $this->_visit,
                $this->_instrument
            ))->toArray();
        }

        return new \LORIS\Http\Response\JsonResponse(
            $body
        );
    }

    /**
     * Handles a PUT request
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handlePUT(ServerRequestInterface $request) : ResponseInterface
    {
        if (!$this->_instrument->determineDataEntryAllowed()) {
            return new \LORIS\Http\Response\JSON\Forbidden(
                'Can not update instruments that are flagged as complete.'
            );
        }

        $data = json_decode((string) $request->getBody(), true);
        if (!is_array($data)) {
            return new \LORIS\Http\Response\JSON\BadRequest(
                'Invalid request'
            );
        }

        if (!$this->_instrument->validate($data)) {
            return new \LORIS\Http\Response\JSON\Forbidden(
                'Could not update.'
            );
        }

        try {
            $this->_instrument->clearInstrument();
            $version = $request->getAttribute('LORIS-API-Version');
            if ($version == 'v0.0.3') {
                $instrumentname = $this->_instrument->testName;
                $this->_instrument->_saveValues($data[$instrumentname]);
            } else {
                $this->_instrument->_saveValues($data['Data']);
            }
            $this->_instrument->score();
            $this->_instrument->updateRequiredElementsCompletedFlag();
        } catch (\Throwable $e) {
            error_log($e->getMessage());
            return new \LORIS\Http\Response\JSON\InternalServerError();
        }
        return (new \LORIS\Http\Response\JSON\NoContent());
    }

    /**
     * Handles a PATCH request.
     * Same as handlePatch but the instrument is not cleared before save
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handlePATCH(ServerRequestInterface $request): ResponseInterface
    {
        if (!$this->_instrument->determineDataEntryAllowed()) {
            return new \LORIS\Http\Response\JSON\Forbidden(
                'Can not update instruments that are flagged as complete.'
            );
        }

        $data = json_decode((string) $request->getBody(), true);
        if (!is_array($data)) {
            return new \LORIS\Http\Response\JSON\BadRequest(
                'Invalid request'
            );
        }

        if (!$this->_instrument->validate($data)) {
            return new \LORIS\Http\Response\JSON\Forbidden(
                'Could not update.'
            );
        }

        try {
            $version = $request->getAttribute('LORIS-API-Version');
            if ($version == 'v0.0.3') {
                $instrumentname = $this->_instrument->testName;
                $this->_instrument->_saveValues($data[$instrumentname]);
            } else {
                $this->_instrument->_saveValues($data['Data']);
            }
            $this->_instrument->score();
            $this->_instrument->updateRequiredElementsCompletedFlag();
        } catch (\Throwable $e) {
            error_log($e->getMessage());
            return new \LORIS\Http\Response\JSON\InternalServerError();
        }
        return (new \LORIS\Http\Response\JSON\NoContent());
    }

    /**
     * Implements the ETagCalculator interface
     *
     * @param ServerRequestInterface $request The PSR7 incoming request.
     *
     * @return string etag summarizing value of this request.
     */
    public function ETag(ServerRequestInterface $request) : string
    {
        return md5($this->_instrument->toJSON());
    }
}

